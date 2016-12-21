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
#include "tcp-event-client.hpp"

void EventClient::add(std::string hostname, uint16_t port){
	this->connections.push_back(new Connection(hostname, port));
}

void EventClient::close_client(Connection* conn){
	if(close(conn->fd) < 0){
		ERROR("close")
	}
	conn->state = DEAD;
}

void EventClient::run(std::function<bool(int, const char*, ssize_t)> new_on_read){
	char packet[PACKET_LIMIT];
	bool running = true;

	this->on_read = new_on_read;

	while(running){
	if(this->on_event_loop != nullptr){
		this->on_event_loop();
	}
	running = false;
	for(auto& connection : this->connections){
		switch(connection->state){
		case DISCONNECTED:
			if((connection->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
				ERROR("socket")
				this->close_client(connection);
				break;
			}
			Util::set_non_blocking(connection->fd);
			if(!this->host_addresses.count(connection->hostname)){
				struct hostent* host;
				if((host = gethostbyname(connection->hostname.c_str())) == 0){
					ERROR("gethostbyname")
					this->close_client(connection);
					break;
				}
				this->host_addresses[connection->hostname] = *reinterpret_cast<struct in_addr*>(host->h_addr);
			}
			connection->server_address->sin_family = AF_INET;
			connection->server_address->sin_addr = this->host_addresses[connection->hostname];
			connection->server_address->sin_port = htons(connection->port);
			connection->state = CONNECTING;
			running = true;
			break;
		case CONNECTING:
			bzero(&(connection->server_address->sin_zero), 8);
			if(connect(connection->fd, reinterpret_cast<struct sockaddr*>(connection->server_address), sizeof(struct sockaddr_in)) < 0){
				if(errno !=  EINPROGRESS && errno != EALREADY){
					ERROR("connect")
					this->close_client(connection);
					break;
				}
				// Still connecting, cooooool.
			}else{
				connection->state = CONNECTED;
				if(this->on_connect != nullptr){
					if(this->on_connect(connection->fd)){
						this->close_client(connection);
					}
				}
			}
			running = true;
			break;
		case CONNECTED:
			if(this->recv(connection->fd, packet, PACKET_LIMIT)){
				this->close_client(connection);
			}
			running = true;
			break;
		case DEAD:
			break;
		}
	}
	}
}

bool EventClient::recv(int fd, char* data, size_t data_length){
	ssize_t len;
	if((len = read(connection->fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK){
			ERROR("read")
			return true;
		}
		// Nothing to read, coooooool.
		return false;
	}else if(len == 0){
		ERROR("client read zero")
		return true;
	}else{
		packet[len] = 0;
		return this->on_read(connection->fd, packet, len);
	}
}
