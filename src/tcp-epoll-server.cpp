#include "util.hpp"
#include "stack.hpp"
#include "tcp-epoll-server.hpp"

EpollServer::EpollServer(uint16_t port, size_t new_max_connections, std::string new_name)
:EventServer(port, new_max_connections, new_name),
client_events(new struct epoll_event[this->max_connections]){
	if((this->epoll_fd = epoll_create1(0)) < 0){
		perror("epoll_create1");
		throw std::runtime_error(this->name + " epoll_create1");
	}
	this->new_event.events = EPOLLIN;
	this->new_event.data.fd = this->server_fd;
	if(epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->server_fd, &this->new_event) < 0){
		perror("epoll_ctl");
		throw std::runtime_error(this->name + " epoll_ctl");
	}
}

void EpollServer::run_thread(unsigned int){
	int num_fds, new_fd, i;
	char packet[PACKET_LIMIT];
	ssize_t len;
	bool running = true;

	while(running){
		if((num_fds = epoll_wait(this->epoll_fd, this->client_events, static_cast<int>(this->max_connections), -1)) < 0){
			perror("epoll_wait");
			sleep(1);
			running = false;
			continue;
		}
		for(i = 0; i < num_fds; ++i){
			if(this->client_events[i].data.fd == this->server_fd){
				while(true){
					if((new_fd = accept(this->server_fd, 0, 0)) < 0){
						if(errno != EWOULDBLOCK){
							perror("accept");
							sleep(1);
							break;
						}
						break;
					}
					Util::set_non_blocking(new_fd);
					this->new_event.events = EPOLLIN | EPOLLET;
					this->new_event.data.fd = new_fd;
					if(epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, new_fd, &this->new_event) < 0){
						perror("epoll_ctl");
						sleep(1);
						continue;
					}
					if(this->on_connect != nullptr){
						this->on_connect(new_fd);
					}
				}
			}else{
				if((len = this->recv(this->client_events[i].data.fd, packet, PACKET_LIMIT)) < 0){
					if(close(this->client_events[i].data.fd) < 0){
						perror("close");
						sleep(1);
					}
					if(this->on_disconnect != nullptr){
						this->on_disconnect(this->client_events[i].data.fd);
					}
				}else if(len > 0){
					if(len == PACKET_LIMIT){
						ERROR("more to read uh oh!")
					}
				}else{
					ERROR("read zero!")
				}
			}
		}
	}
}
