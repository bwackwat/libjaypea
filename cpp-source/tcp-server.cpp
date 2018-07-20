#include "util.hpp"
#include "stack.hpp"
#include "tcp-server.hpp"

/**
 * @brief A TCP, epoll-based, IPv4 server constructor.
 * @param port The port number the server's socket will bind to.
 * @param new_max_connections The maximum number of connections *each thread* can handle.
 * Also the listen backlog.
 * @param new_name An arbitrary name for debugging purposes.
 *
 * This is the essential networking server code. Architecturally, each thread calls epoll_wait() and
 * waits for either a mutexed server fd to accept a new connection, a connected fd to indicate there is data to read,
 * or for a connected fd to timeoutvia timerfd. Each thread manages its own set of connected fds, timeout fds, and
 * is able to accept new connections because epoll_wait is thread safe for reused fds and there is a mutex.
 * @see EpollServer::accept_mutex
 *
 * @bug There lies a type of race condition within a single thread where a timeout and a read event can both trigger.
 * In some cases I believe this is harmless, in other cases I wouldn't be suprised if some memory leaks or other
 * unexpected behavior occurs. Need to research or reevaluate how epoll errors should be treated. EDIT: Resolved?
 * This was solved by using EPOLL_MOD_DEL on the timed-out client's id.
 * @bug EpollServer::start_event does nothing. EDIT: Resolved. A pipe is created to write event data to, and read from
 * in epoll where the broadcast occurs.
 * @bug There is a condition within the kernel where not all data sent to write() is written.
 * This is completely unaccounted for, but I havent hit it yet.
 */
EpollServer::EpollServer(uint16_t port, size_t new_max_connections, std::string new_name)
:name(new_name),
max_connections(new_max_connections),
timeout(10),
sockaddr_length(sizeof(this->accept_client)),
running(true){
	if((this->server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		throw std::runtime_error(this->name + " socket");
	}
	Util::set_non_blocking(this->server_fd);

	if(pipe2(this->broadcast_pipe, O_DIRECT | O_NONBLOCK) < 0){
		perror("pipe2");
		throw std::runtime_error(this->name + " pipe2");
	}

/*
	The setsockopt() call below is unnecessary, but allows for quicker development.
	With this, the server can be stopped and restarted on the same port immediately.
*/
	int opt = 1;
	if(setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) < 0){
		perror("setsockopt");
		throw std::runtime_error(this->name + " setsockopt");
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	if(bind(this->server_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0){
		perror("bind");
		throw std::runtime_error(this->name + " bind");
	}
	if(listen(this->server_fd, static_cast<int>(this->max_connections)) < 0){
		perror("listen");
		throw std::runtime_error(this->name + " listen");
	}
	PRINT(this->name << " listening on " << port)
}

/**
 * @brief The base function for closing a client. Important for the PrivateEpollServer implementation.
 *
 * @param fd The fd that is closing.
 * @param callback The thread-specific callback. *I think* this is only necessary because of thread-specific variables.
 */
void EpollServer::close_client(int* fd, std::function<void(int*)> callback){
	callback(fd);
}

bool EpollServer::send(int fd, std::string data){
	return this->send(fd, data.c_str(), static_cast<size_t>(data.length()));
}

/**
 * @brief The base send function that should be used by all implementers.
 *
 * @param fd The fd to write to.
 * @param data the data to write.
 * @param data_length the length of the data that *should* be written.
 *
 * @return true on error.
 */
bool EpollServer::send(int fd, const char* data, size_t data_length){
	ssize_t len;
	PRINT("SEND ON TCP")
	if((len = write(fd, data, data_length)) < 0){
		perror("write");
		ERROR("send")
		return true;
	}
	if(static_cast<size_t>(len) != data_length){
		ERROR("NOT ALL THE DATA WAS SENT TO " << fd)
	}
	this->write_counter[fd]++;
	return false;
}

/**
 * @brief The base receive function that @see EpollServer::run_thread calls.
 *
 * @param fd The fd which has data to read.
 * @param data The character array data will be read to.
 * @param data_length The number of bytes read.
 *
 * @return Negative on error, zero on nothing to read, and positive for successful read.
 */
ssize_t EpollServer::recv(int fd, char* data, size_t data_length){
	return this->recv(fd, data, data_length, this->on_read);
}

ssize_t EpollServer::recv(int fd, char* data, size_t data_length,
std::function<ssize_t(int, char*, size_t)> callback){
	ssize_t len;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK && errno != EAGAIN){
			perror("read");
			ERROR(this->name << " on " << fd)
			return -1;
		}
		return 0;
	}else if(len == 0){
		return -2;
	}else{
		data[len] = 0;
		return callback(fd, data, static_cast<size_t>(len));
	}
}

/**
 * @brief The base continuation function for accepting a connection.
 *
 * @return true If the fd should actually be closed.
 *
 * This is only *necessary* for @see PrivateEpollServer::accept_continuation.
 */
bool EpollServer::accept_continuation(int*){
	return false;
}

/**
 * Protects any messing with the broadcasting pipe.
 * Use this to broadcast some data to all connected fds!
 *
 * @return The write-end of an internal broadcasting pipe, where epoll uses the read-end.
 */
int EpollServer::broadcast_fd(){
	return this->broadcast_pipe[1];
}

/// Sets the timeout. This is dynamic, yet not unique per connection.
void EpollServer::set_timeout(time_t seconds){
	this->timeout = seconds;
}

/**
 * @brief The essential threaded epoll server function. Starts via std::thread.
 *
 * @param thread_id The id of the thread. Necessary for potential events.
 */
void EpollServer::run_thread(unsigned int thread_id){
	int num_fds, num_timeouts, new_fd, the_fd, timer_fd, i, j, epoll_fd, timeout_epoll_fd;
	unsigned long num_connections = 0;
	char packet[PACKET_LIMIT + 32];
	ssize_t len;
	struct itimerspec timer_spec;
	
	//struct timeval recv_timeout;

	struct epoll_event new_event;
	struct epoll_event* client_events = new struct epoll_event[this->max_connections + 4];
						// + 1 for server_fd
						// + 1 for timeout_epoll_fd
						// + 1 for broadcast_pipe[0]
						// + 1 for fun
	struct epoll_event* timeout_events = new struct epoll_event[this->max_connections];
	
	std::unordered_map<int /* timer fd */, int /* client fd */> timer_to_client_map;
	std::unordered_map<int /* client fd */, int /* timer fd */> client_to_timer_map;
	char client_detail[INET_ADDRSTRLEN];

	// Broken pipes will make SSL_write (or any write, actually) return with an error instead of interrupting the program.
	signal(SIGPIPE, SIG_IGN);

	if((epoll_fd = epoll_create1(0)) < 0){
		perror("epoll_create1");
		throw std::runtime_error(this->name + " epoll_create1");
	}

	// Add server_fd
	memset(&new_event, 0, sizeof(new_event));
	new_event.events = EVENTS;
	new_event.data.fd = this->server_fd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, this->server_fd, &new_event) < 0){
		perror("epoll_ctl");
		throw std::runtime_error(this->name + " epoll_ctl");
	}

	if((timeout_epoll_fd = epoll_create1(0)) < 0){
		perror("epoll_create1 timeout");
		throw std::runtime_error(this->name + " epoll_create1 timeout");
	}

	// Add timeout_epoll_fd
	memset(&new_event, 0, sizeof(new_event));
	new_event.events = EVENTS;
	new_event.data.fd = timeout_epoll_fd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timeout_epoll_fd, &new_event) < 0){
		perror("epoll_ctl timeout");
		throw std::runtime_error(this->name + " epoll_ctl timeout");
	}

	// Add broadcast_pipe[0]
	memset(&new_event, 0, sizeof(new_event));
	new_event.events = EVENTS;
	new_event.data.fd = this->broadcast_pipe[0];
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, this->broadcast_pipe[0], &new_event) < 0){
		perror("epoll_ctl timeout broadcast");
		throw std::runtime_error(this->name + " epoll_ctl broadcast");
	}

	std::function<void(int*)> close_client_callback = [&](int* fd){
		if(close(*fd) < 0){
			perror("close");
		}
		DEBUG(this->name << ": " << *fd << " done on " << thread_id)
		if(this->on_disconnect != nullptr){
			this->on_disconnect(*fd);
		}
		if(num_connections >= this->max_connections){
			new_event.events = EVENTS;
			new_event.data.fd = this->server_fd;
			if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, this->server_fd, &new_event) < 0){
				perror("epoll_ctl add server");
			}
		}
		num_connections--;
	};

	while(this->running){
		if((num_fds = epoll_wait(epoll_fd, client_events, static_cast<int>(this->max_connections + 2), 10000)) < 0){
			if(errno == EINTR){
				PRINT("EINTR!")
				continue;
			}
			perror("epoll_wait");
			this->running = false;
			continue;
		}
		if(num_fds == 0){
			//DEBUG("EPOLL_WAIT TIMEOUT")
			continue;
		}
		for(i = 0; i < num_fds; ++i){
			the_fd = client_events[i].data.fd;
			if(client_events[i].events & EPOLLERR){
				PRINT("EPOLLERR: " << the_fd)
			}else if(client_events[i].events & EPOLLHUP){
				PRINT("EPOLLHUP: " << the_fd)
			}else if(!(client_events[i].events & EPOLLIN)){
				PRINT("EPOLLIN: " << the_fd)
			}
			if((client_events[i].events & EPOLLERR) || 
			(client_events[i].events & EPOLLHUP) || 
			(!(client_events[i].events & EPOLLIN))){
				timer_fd = client_to_timer_map[the_fd];
				client_to_timer_map.erase(the_fd);
				timer_to_client_map.erase(timer_fd);
				ERROR(the_fd << " and timer " << timer_fd << " on error.")
				if(close(timer_fd) < 0){
					perror("close timer_fd on error");
				}
				this->close_client(&the_fd, close_client_callback);
				continue;
			}
			if(the_fd == timeout_epoll_fd){
				if((num_timeouts = epoll_wait(timeout_epoll_fd, timeout_events, static_cast<int>(this->max_connections), 1000)) < 0){
					perror("epoll_wait timeout");
					continue;
				}else if(num_timeouts > 0){
					for(j = 0; j < num_timeouts; ++j){
						timer_fd = timeout_events[j].data.fd;
						new_fd = timer_to_client_map[timer_fd];
						timer_to_client_map.erase(timer_fd);
						client_to_timer_map.erase(new_fd);
						DEBUG(this->name << ": " << timer_fd << " timed out on " << new_fd)
						if(close(timer_fd) < 0){
							perror("close timer_fd on timeout");
							continue;
						}
						if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, new_fd, 0) < 0){
							perror("epoll_ctl del timedout fd");
						}
						this->close_client(&new_fd, close_client_callback);
					}
				}else{
					ERROR("NO TIMEOUTS WHY DID EVENT FIRE?")
				}
				client_events[i].events = EVENTS;
				client_events[i].data.fd = timeout_epoll_fd;
				if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, timeout_epoll_fd, &client_events[i]) < 0){
					perror("epoll_ctl mod timeout");
				}
			}else if(the_fd == this->broadcast_pipe[0]){
				/*
					This is an implementation of broadcasting within epoll.	The read-end of EpollServer::broadcast_pipe is added to epoll,
					so when	an implementation calls EpollServer::send(EpollServer::broadcast_fd(), data, data_length)
					(which writes to broadcast_pipe[1]), each thread will receive a new event landing here. Data
					is first read from the read end of the pipe, and then written to each currently tracked client fd. Broadcasted.
				*/
				if((len = read(this->broadcast_pipe[0], packet, PACKET_LIMIT)) <= 0){
					ERROR("I really hope this doesn't happen.")
				}
				packet[len] = 0;
				for(auto iter = client_to_timer_map.begin(); iter != client_to_timer_map.end(); ++iter){
					// Throw away send errors?
					if(this->send(iter->first, packet, static_cast<size_t>(len))){
						ERROR("failed to broadcast to " << iter->first)
					}
				}
				client_events[i].events = EVENTS;
				client_events[i].data.fd = this->broadcast_pipe[0];
				if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, this->broadcast_pipe[0], &client_events[i]) < 0){
					perror("epoll_ctl mod broadcast");
				}
			}else if(the_fd == this->server_fd){
				if(this->accept_mutex.try_lock()){
					while(num_connections < this->max_connections){
						if((new_fd = accept(this->server_fd, reinterpret_cast<struct sockaddr*>(&this->accept_client), &this->sockaddr_length)) < 0){
							if(errno != EWOULDBLOCK && errno != EAGAIN){
								perror("accept");
								running = false;
							}else{
								#if defined(DO_DEBUG)
									perror("accept nonblock");
								#endif
							}
							break;
						}else{
							if(inet_ntop(AF_INET, &this->accept_client.sin_addr.s_addr, client_detail, sizeof(client_detail)) != 0){
								PRINT(this->name << ": Connection from " << client_detail)
								this->fd_to_details_map[new_fd] = std::string(client_detail);
							}else{
								perror("inet_ntop!");
								this->fd_to_details_map[new_fd] = "NULL";
							}
							Util::set_non_blocking(new_fd);
							if(this->accept_continuation(&new_fd)){
								DEBUG("accept continuation failed " << new_fd)
								this->close_client(&new_fd, [&](int* fd){
									close(*fd);
								});
								break;
							}
							new_event.events = EVENTS;
							new_event.data.fd = new_fd;
							if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &new_event) < 0){
								perror("epoll_ctl add client");
								this->close_client(&new_fd, [&](int* fd){
									close(*fd);
								});
								break;
							}else{
								if((timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) < 0){
									perror("timerfd_create");
								}else{
									timer_spec.it_interval.tv_sec = 0;
									timer_spec.it_interval.tv_nsec = 0;
									timer_spec.it_value.tv_sec = this->timeout;
									timer_spec.it_value.tv_nsec = 0;
									if(timerfd_settime(timer_fd, 0, &timer_spec, 0) < 0){
										perror("timerfd_settime");
									}else{
										timer_to_client_map[timer_fd] = new_fd;
										client_to_timer_map[new_fd] = timer_fd;
										new_event.events = EVENTS;
										new_event.data.fd = timer_fd;
										DEBUG(this->name << ": new tfd " << timer_fd)
										if(epoll_ctl(timeout_epoll_fd, EPOLL_CTL_ADD, timer_fd, &new_event) < 0){
											perror("epoll_ctl timer add");
										}
									}
								}
								/*
								I found some docs that said this should work (and thus replace all the timerfds), but it doesn't?

								recv_timeout.tv_sec = 10;
								recv_timeout.tv_usec = 0;
								if(setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&recv_timeout), sizeof(struct timeval)) < 0){
									perror("setsockopt new connection timeout");
								}
								*/
								this->read_counter[new_fd] = 0;
								this->write_counter[new_fd] = 0;
								if(this->on_connect != nullptr){
									this->on_connect(new_fd);
								}
								num_connections++;
							}
						}
					}
					if(num_connections < this->max_connections){
						client_events[i].events = EVENTS;
						client_events[i].data.fd = this->server_fd;
						if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, this->server_fd, &client_events[i]) < 0){
							perror("epoll_ctl mod server");
						}
					}else{
						if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, this->server_fd, &client_events[i]) < 0){
							perror("epoll_ctl del server");
						}
					}
					this->accept_mutex.unlock();
				}
			}else{
				timer_fd = client_to_timer_map[the_fd];
				timer_spec.it_interval.tv_sec = 0;
				timer_spec.it_interval.tv_nsec = 0;
				timer_spec.it_value.tv_sec = this->timeout;
				timer_spec.it_value.tv_nsec = 0;
				if(timerfd_settime(timer_fd, 0, &timer_spec, 0) < 0){
					PRINT("timerfd_setting error " << the_fd << " and timer " << timer_fd)
					perror("timerfd_settime update");
				}else if((len = this->recv(the_fd, packet, PACKET_LIMIT)) < 0){
					timer_to_client_map.erase(timer_fd);
					client_to_timer_map.erase(the_fd);
					PRINT(this->name << ": " << the_fd << " and timer " << timer_fd << " donezo.")
					if(close(timer_fd) < 0){
						perror("close timer_fd");
					}
					this->close_client(&the_fd, close_client_callback);
				}else if(len >= PACKET_LIMIT){
					ERROR("OVERFLOAT more to read uh oh!")
				}else if(len == 0){
					PRINT("RECEIVE ZERO?!")
				}else{
					client_events[i].events = EVENTS;
					client_events[i].data.fd = the_fd;
					if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, the_fd, &client_events[i]) < 0){
						perror("epoll_ctl mod client");
					}
				}
			}
		}
	}
}

/**
 * @brief Starts the server.
 *
 * @param returning If this is true, the function will return after spinning some threads.
 * @param new_num_threads Creates this many threads. If *returning* is true, the number of threads that start will not change.
 *
 * E.g. server.run(true, 2), starts 2 threads, but continues.
 * server.run(false, 3) starts 3 threads, but one of those threads is *this* thread.
 */
void EpollServer::run(bool returning, unsigned int new_num_threads){
	this->num_threads = new_num_threads;
	if(this->num_threads <= 0){
		this->num_threads = 1;
	}

	unsigned int total = this->num_threads;
	if(!returning){
		total--;
	}

	for(unsigned int i = 0; i < total; ++i){
			std::thread next(&EpollServer::run_thread, this, i);
			next.detach();
	}

	if(returning){
		return;
	}

	this->run_thread(total);
}
