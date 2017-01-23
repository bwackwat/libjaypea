#include <stdexcept>
#include <string>
#include <cstring>
#include <iostream>
#include <csignal>
#include <typeinfo>
#include <cstdio>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.hpp"
#include "tcp-client.hpp"

void EventClient::add(Connection* conn){
	this->connections.push_back(conn);
	this->alive_connections++;
}

void EventClient::close_client(Connection* conn){
	if(close(conn->fd) < 0){
		perror("close");
	}
	conn->state = DEAD;
	this->alive_connections--;
}

void EventClient::run(std::function<ssize_t(int, const char*, size_t)> new_on_read){
	char packet[PACKET_LIMIT];
	bool running = true;
	ssize_t len;

	this->on_read = new_on_read;

	while(running){
		if(this->on_event_loop != nullptr){
			if(this->on_event_loop()){
				break;
			}
		}
		running = false;
		for(Connection* connection : this->connections){
			switch(connection->state){
			case DISCONNECTED:
				if((connection->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
					if(errno == EMFILE){
						continue;
					}else{	
						perror("socket");
						PRINT("errno " << errno);
						this->close_client(connection);
						break;
					}
				}
				Util::set_non_blocking(connection->fd);
				if(!connection->hostname.empty()){
					PRINT("Using hostname " << connection->hostname)
					if(!this->host_addresses.count(connection->hostname)){
						struct hostent* host;
						if((host = gethostbyname(connection->hostname.c_str())) == 0){
							perror("gethostbyname");
							this->close_client(connection);
							break;
						}
						this->host_addresses[connection->hostname] = *reinterpret_cast<struct in_addr*>(host->h_addr);
					}
					connection->server_address->sin_family = AF_INET;
					connection->server_address->sin_addr = this->host_addresses[connection->hostname];
					connection->server_address->sin_port = htons(connection->port);
				}else{
					PRINT("Using IP Address " << connection->ip_address)
					connection->server_address->sin_family = AF_INET;
					connection->server_address->sin_addr.s_addr = inet_addr(connection->ip_address.c_str());
					connection->server_address->sin_port = htons(connection->port);
				}
				connection->state = CONNECTING;
				running = true;
				break;
			case CONNECTING:
				bzero(&(connection->server_address->sin_zero), 8);
				if(connect(connection->fd, reinterpret_cast<struct sockaddr*>(connection->server_address), sizeof(struct sockaddr_in)) < 0){
					if(errno !=  EINPROGRESS && errno != EALREADY){
						perror("connect");
						this->close_client(connection);
						break;
					}
					// Still connecting, cooooool.
				}else{
					connection->state = CONNECTED;
					this->read_counter[connection->fd] = 0;
					this->write_counter[connection->fd] = 0;
					if(this->on_connect != nullptr){
						if(this->on_connect(connection->fd)){
							this->close_client(connection);
						}
					}
				}
				running = true;
				break;
			case CONNECTED:
				if((len = this->recv(connection->fd, packet, PACKET_LIMIT)) < 0){
					this->close_client(connection);
				}else if(len > 0){
					this->read_counter[connection->fd]++;
				}
				running = true;
				break;
			case DEAD:
				break;
			}
		}
		if(this->alive_connections <= 0){
			break;
		}
	}
}

bool EventClient::send(int fd, const char* data, size_t data_length){
	if(write(fd, data, data_length) < 0){
		perror("write");
		ERROR("send")
		return true;
	}
	this->write_counter[fd]++;
	return false;
}

ssize_t EventClient::recv(int fd, char* data, size_t data_length){
	ssize_t len;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK){
			perror("read");
			ERROR("read")
			return -1;
		}
		// Nothing to read, coooooool.
		return 0;
	}else if(len == 0){
		ERROR("client read zero")
		return -2;
	}else{
		data[len] = 0;
		return this->on_read(fd, data, len);
	}
}
