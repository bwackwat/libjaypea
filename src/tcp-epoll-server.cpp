#include "sys/timerfd.h"
#include "signal.h"

#include "util.hpp"
#include "stack.hpp"
#include "tcp-epoll-server.hpp"

#define EVENTS EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLHUP

EpollServer::EpollServer(uint16_t port, size_t new_max_connections, std::string new_name)
:EventServer(port, new_max_connections, new_name){}

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
								close(new_fd);
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
