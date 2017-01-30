#include "util.hpp"
#include "stack.hpp"
#include "tcp-server.hpp"

EpollServer::EpollServer(uint16_t port, size_t new_max_connections, std::string new_name)
:name(new_name),
max_connections(new_max_connections),
running(true){
	if((this->server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		throw std::runtime_error(this->name + " socket");
	}
	Util::set_non_blocking(this->server_fd);
/*
	For this class, I am unsure about what this is genuinely useful for...
	However, it has been useful in the past and I have read it as "good practice."
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

void EpollServer::start_event(Event* event){
	for(unsigned int i = 0; i < this->num_threads; ++i){
		this->thread_event_queue_mutexes[i].lock();
		this->thread_event_queues[i].enqueue(event);
		this->thread_event_queue_mutexes[i].unlock();
	}
}

void EpollServer::close_client(size_t index, int* fd, std::function<void(size_t, int*)> callback){
	callback(index, fd);
}

bool EpollServer::send(int fd, const char* data, size_t data_length){
	ssize_t len;
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

ssize_t EpollServer::recv(int fd, char* data, size_t data_length){
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
		return this->on_read(fd, data, len);
	}
}

bool EpollServer::accept_continuation(int*){
	return false;
}

void EpollServer::run_thread(unsigned int thread_id){
	int num_fds, num_timeouts, new_fd, the_fd, timer_fd, i, j, epoll_fd, timeout_epoll_fd;
	unsigned long num_connections = 0;
	char packet[PACKET_LIMIT];
	ssize_t len;
	struct itimerspec timer_spec;

	struct epoll_event new_event;
	struct epoll_event* client_events = new struct epoll_event[this->max_connections + 2];
						// + 2 for server_fd and timeout_epoll_fd
	struct epoll_event* timeout_events = new struct epoll_event[this->max_connections];
	
	std::unordered_map<int /* timer fd */, int /* client fd */> timer_to_client_map;
	std::unordered_map<int /* client fd */, int /* timer fd */> client_to_timer_map;

	// Broken pipes will make SSL_write (or any write, actually) return with an error instead of interrupting the program.
	signal(SIGPIPE, SIG_IGN);

	if((epoll_fd = epoll_create1(0)) < 0){
		perror("epoll_create1");
		throw std::runtime_error(this->name + " epoll_create1");
	}
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
	new_event.events = EVENTS;
	new_event.data.fd = timeout_epoll_fd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timeout_epoll_fd, &new_event) < 0){
		perror("epoll_ctl timeout");
		throw std::runtime_error(this->name + " epoll_ctl timeout");
	}

	std::function<void(size_t, int*)> close_client_callback = [&](size_t, int* fd){
		if(close(*fd) < 0){
			perror("close");
		}
		PRINT(*fd << " done on " << thread_id)
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
		if((num_fds = epoll_wait(epoll_fd, client_events, static_cast<int>(this->max_connections + 2), -1)) < 0){
			if(errno == EINTR){
				continue;
			}
			perror("epoll_wait");
			this->running = false;
			continue;
		}
		for(i = 0; i < num_fds; ++i){
			the_fd = client_events[i].data.fd;
			if(client_events[i].events & EPOLLERR){
				std::cout << "EPOLLERR ";
			}else if(client_events[i].events & EPOLLHUP){
				std::cout << "EPOLLHUP ";
			}else if(!(client_events[i].events & EPOLLIN)){
				std::cout << "NOT EPOLLIN? ";
			}
			if((client_events[i].events & EPOLLERR) || 
			(client_events[i].events & EPOLLHUP) || 
			(!(client_events[i].events & EPOLLIN))){
				timer_fd = client_to_timer_map[the_fd];
				client_to_timer_map.erase(the_fd);
				timer_to_client_map.erase(timer_fd);
				std::cout << "error, close tfd " << timer_fd << ' ';
				if(close(timer_fd) < 0){
					perror("close timer_fd on error");
				}
				this->close_client(0, &the_fd, close_client_callback);
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
						std::cout << "timeout, close tfd " << timer_fd << ' ';
						if(close(timer_fd) < 0){
							perror("close timer_fd on timeout");
							continue;
						}
						this->close_client(0, &new_fd, close_client_callback);
					}
				}else{
					ERROR("NO TIMEOUTS WHY DID EVENT FIRE?")
				}
				client_events[i].events = EVENTS;
				client_events[i].data.fd = timeout_epoll_fd;
				if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, timeout_epoll_fd, &client_events[i]) < 0){
					perror("epoll_ctl mod timeout");
				}
			}else if(the_fd == this->server_fd){
				if(this->accept_mutex.try_lock()){
					while(num_connections < this->max_connections){
						if((new_fd = accept(this->server_fd, 0, 0)) < 0){
							if(errno != EWOULDBLOCK && errno != EAGAIN){
								perror("accept");
								running = false;
							}
							break;
						}else{
							if(this->accept_continuation(&new_fd)){
								PRINT("accept continuation failed.")
								this->close_client(0, &new_fd, [&](size_t, int* fd){
									close(*fd);
								});
							}else{
								Util::set_non_blocking(new_fd);
								new_event.events = EVENTS;
								new_event.data.fd = new_fd;
								if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &new_event) < 0){
									perror("epoll_ctl add client");
									close(new_fd);
								}else{
									if((timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) < 0){
										perror("timerfd_create");
									}else{
										timer_spec.it_interval.tv_sec = 10;
										timer_spec.it_interval.tv_nsec = 0;
										timer_spec.it_value.tv_sec = 10;
										timer_spec.it_value.tv_nsec = 0;
										//PRINT("TIME " << timer_fd)
										if(timerfd_settime(timer_fd, 0, &timer_spec, 0) < 0){
											perror("timerfd_settime");
										}else{
											timer_to_client_map[timer_fd] = new_fd;
											client_to_timer_map[new_fd] = timer_fd;
											new_event.events = EVENTS;
											new_event.data.fd = timer_fd;
											//PRINT("new tfd " << timer_fd)
											if(epoll_ctl(timeout_epoll_fd, EPOLL_CTL_ADD, timer_fd, &new_event) < 0){
												perror("epoll_ctl timer add");
											}
										}
									}
									this->read_counter[new_fd] = 0;
									this->write_counter[new_fd] = 0;
									if(this->on_connect != nullptr){
										this->on_connect(new_fd);
									}
									num_connections++;
								}
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
				timer_spec.it_interval.tv_sec = 10;
				timer_spec.it_interval.tv_nsec = 0;
				timer_spec.it_value.tv_sec = 10;
				timer_spec.it_value.tv_nsec = 0;
				if(timerfd_settime(timer_fd, 0, &timer_spec, 0) < 0){
					perror("timerfd_settime update");
				}
				if((len = this->recv(the_fd, packet, PACKET_LIMIT)) < 0){
					timer_to_client_map.erase(timer_fd);
					client_to_timer_map.erase(the_fd);
					PRINT("BAD RECV. " << the_fd << " timing " << timer_fd)
					if(close(timer_fd) < 0){
						perror("close timer_fd");
					}
					this->close_client(0, &the_fd, close_client_callback);
				}else if(len == PACKET_LIMIT){
					ERROR("OVERFLOAT more to read uh oh!")
				}else{
					this->read_counter[the_fd]++;
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

void EpollServer::run(bool returning, unsigned int new_num_threads){
	this->num_threads = new_num_threads;
	if(this->num_threads <= 0){
		this->num_threads = 1;
	}
	thread_event_queues = new Queue<Event*>[this->num_threads];
	thread_event_queue_mutexes = new std::mutex[this->num_threads];

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
