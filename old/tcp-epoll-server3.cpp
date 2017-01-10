#include "util.hpp"
#include "stack.hpp"
#include "tcp-epoll-server.hpp"

#define EVENTS EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR

EpollServer::EpollServer(uint16_t port, size_t new_max_connections, std::string new_name)
:EventServer(port, new_max_connections, new_name),
client_events(new struct epoll_event[this->max_connections + 1]),
num_connections(0){
	if((this->epoll_fd = epoll_create1(0)) < 0){
		perror("epoll_create1");
		throw std::runtime_error(this->name + " epoll_create1");
	}
	struct epoll_event next_event;
	next_event.events = EVENTS;
	next_event.data.fd = this->server_fd;
	if(epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->server_fd, &next_event) < 0){
		perror("epoll_ctl");
		throw std::runtime_error(this->name + " epoll_ctl");
	}
}

ssize_t EpollServer::recv(int fd, char* data, size_t data_length){
	ssize_t len;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK && errno != EAGAIN){
			return -1;
		}
		return 0;
	}else if(len == 0){
		return -1;
	}else{
		data[len] = 0;
		return this->on_read(fd, data, len);
	}
}

void EpollServer::run_thread(unsigned int thread_id){
	int num_fds, new_fd, i;
	char packet[PACKET_LIMIT];
	ssize_t len;
	bool running = true;

	PRINT("accept on : " << this->server_fd << " epoll : " << this->epoll_fd)
	while(running){
		//PRINT("poll...")
		if((num_fds = epoll_wait(this->epoll_fd, this->client_events, static_cast<int>(this->max_connections + 1), -1)) < 0){
			perror("epoll_wait");
			running = false;
			continue;
		}
		for(i = 0; i < num_fds; ++i){
			int the_fd = this->client_events[i].data.fd;
			if(!this->fd_mutexes[the_fd].try_lock()){
				continue;
			}
			if(this->client_events[i].events & EPOLLERR){
				std::cout << "EPOLLERR ";
			}
			if(!(this->client_events[i].events & EPOLLIN)){
				std::cout << "NOT EPOLLIN? ";
			}
			if((this->client_events[i].events & EPOLLERR) || 
			(!(this->client_events[i].events & EPOLLIN))){
				PRINT(the_fd << " WHAOOHHHH")
				if(close(the_fd) < 0){
					PRINT("CLOSE OLD " << the_fd)
					perror("close client");
				}
				if(this->on_disconnect != nullptr){
					this->on_disconnect(the_fd);
				}
				if(this->num_connections >= this->max_connections){
					struct epoll_event next_event;
					next_event.events = EVENTS;
					next_event.data.fd = this->server_fd;
					this->epoll_mutex.lock();
					if(epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->server_fd, &next_event) < 0){
						perror("epoll_ctl add server");
					}
					this->epoll_mutex.unlock();
				}
				this->num_connections--;
				continue;
			}
//			PRINT("check " << the_fd << " on " << thread_id)
			if(the_fd == this->server_fd){
				while(this->num_connections < this->max_connections){
					if((new_fd = accept(this->server_fd, 0, 0)) < 0){
						if(errno != EWOULDBLOCK && errno != EAGAIN){
							PRINT(new_fd << " on " << thread_id << " with server: " << this->server_fd)
							perror("accept");
							running = false;
							break;
						}
						PRINT("ACCEPT DONE on " << thread_id)
						break;
					}else{
						PRINT("ACCEPT " << new_fd << " on " << thread_id)
						Util::set_non_blocking(new_fd);
						struct epoll_event next_event;
						next_event.events = EVENTS;
						next_event.data.fd = new_fd;
						this->epoll_mutex.lock();
						if(epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, new_fd, &next_event) < 0){
							perror("epoll_ctl add client");
							close(new_fd);
						}else{
							this->num_connections++;
							if(this->on_connect != nullptr){
								this->on_connect(new_fd);
							}
						}
						this->epoll_mutex.unlock();
					}
				}
				this->epoll_mutex.lock();
				if(this->num_connections < this->max_connections){
					this->client_events[i].events = EVENTS;
					this->client_events[i].data.fd = this->server_fd;
					if(epoll_ctl(this->epoll_fd, EPOLL_CTL_MOD, this->server_fd, &this->client_events[i]) < 0){
						perror("epoll_ctl mod server");
					}
				}else{
					if(epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, this->server_fd, &this->client_events[i]) < 0){
						perror("epoll_ctl del server");
					}
				}
				this->epoll_mutex.unlock();
			}else{
				PRINT(the_fd << " READ on " << thread_id)
				while(1){
					if((len = this->recv(the_fd, packet, PACKET_LIMIT)) < 0){
						PRINT(the_fd << " CLOSE on " << thread_id)
			//		if(epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, the_fd, &this->client_events[i]) < 0){
			//			perror("epoll_ctl del client");
			//		}
						if(close(the_fd) < 0){
							PRINT("CLOSE OLD " << the_fd)
							perror("close client");
						}
						if(this->on_disconnect != nullptr){
							this->on_disconnect(the_fd);
						}
						if(this->num_connections >= this->max_connections){
							struct epoll_event next_event;
							next_event.events = EVENTS;
							next_event.data.fd = this->server_fd;
							this->epoll_mutex.lock();
							if(epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->server_fd, &next_event) < 0){
								perror("epoll_ctl add server");
							}
							this->epoll_mutex.unlock();
						}
						this->num_connections--;
					}else if(len == PACKET_LIMIT){
						ERROR("OVERFLOAT more to read uh oh!")
					}else if(len >= 0){
						PRINT(the_fd << " COMPLETE on " << thread_id)
	//			}else{
						this->client_events[i].events = EVENTS;
						this->client_events[i].data.fd = the_fd;
						this->epoll_mutex.lock();
						if(epoll_ctl(this->epoll_fd, EPOLL_CTL_MOD, the_fd, &this->client_events[i]) < 0){
						 	perror("epoll_ctl mod client");
						}
						this->epoll_mutex.unlock();
					}else{
						PRINT(the_fd << " REPEAT on " << thread_id)
						continue;
					}
					break;
				}
			}
			this->fd_mutexes[the_fd].unlock();
		}
	}
}
