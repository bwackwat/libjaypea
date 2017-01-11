#include "util.hpp"
#include "stack.hpp"
#include "tcp-epoll-server.hpp"

#define EVENTS EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR

EpollServer::EpollServer(uint16_t port, size_t new_max_connections, std::string new_name)
:EventServer(port, new_max_connections, new_name){}

void EpollServer::run_thread(unsigned int thread_id){
	int num_fds, new_fd, i, epoll_fd;
	unsigned long num_connections = 0;
	char packet[PACKET_LIMIT];
	ssize_t len;
	struct epoll_event new_event;
	struct epoll_event* client_events = new struct epoll_event[this->max_connections];

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
		if((num_fds = epoll_wait(epoll_fd, client_events, static_cast<int>(this->max_connections + 1), -1)) < 0){
			perror("epoll_wait");
			this->running = false;
			continue;
		}
		for(i = 0; i < num_fds; ++i){
			int the_fd = client_events[i].data.fd;
			if(client_events[i].events & EPOLLERR){
				std::cout << "EPOLLERR ";
			}
			if(!(client_events[i].events & EPOLLIN)){
				std::cout << "NOT EPOLLIN? ";
			}
			if((client_events[i].events & EPOLLERR) || 
			(!(client_events[i].events & EPOLLIN))){
				this->close_client(0, &the_fd, close_client_callback);
				continue;
			}
//			PRINT("check " << the_fd << " on " << thread_id)
			if(the_fd == this->server_fd){
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
				if((len = this->recv(the_fd, packet, PACKET_LIMIT)) < 0){
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
