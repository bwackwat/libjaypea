#include "util.hpp"
#include "stack.hpp"
#include "tcp-epoll-server.hpp"

EpollServer::EpollServer(uint16_t port, size_t new_max_connections, std::string new_name)
:max_connections(new_max_connections),
name(new_name),
client_events(new struct epoll_event[this->max_connections]){
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
	if(listen(this->server_fd, 1024) < 0){
		perror("listen");
		throw std::runtime_error(this->name + " listen");
	}
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
	PRINT(this->name << " listening on " << port)
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

bool EpollServer::send(int fd, const char* data, size_t data_length){
	if(write(fd, data, data_length) < 0){
		ERROR("send")
		return true;
	}
	return false;
}

ssize_t EpollServer::recv(int fd, char* data, size_t data_length){
	ssize_t len;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK){
			ERROR(this->name << " read")
			return -1;
		}
		return 0;
	}else if(len == 0){
		ERROR("server read zero")
		return -2;
	}else{
		data[len] = 0;
		return this->on_read(fd, data, static_cast<size_t>(len));
	}
}

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

/*
	if(close(this->server_fd) < 0){
		ERROR("close server_fd")
	}
	for(size_t i = 0; i < this->max_connections; ++i){
		if(this->client_fds[i] != -1){
			if(close(this->client_fds[i]) < 0){
				ERROR("close")
			}
		}
	}
*/
}

void EpollServer::start_event(){
}
