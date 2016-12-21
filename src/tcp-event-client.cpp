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
	struct Connection* new_conn = nullptr;
	*new_conn = {-1, hostname, port, DISCONNECTED, new struct sockaddr_in()};
	this->connections.push_back(new_conn);
}

void EventClient::run(std::function<bool(int, const char*, ssize_t)> new_on_read){
	char packet[PACKET_LIMIT];
	bool running = true;

	this->on_read = new_on_read;

	while(running){
	running = false;
	for(auto& connection : this->connections){
		switch(connection->state){
		case DISCONNECTED:
			if((connection->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
				ERROR("socket")
				connection->state = DEAD;
				break;
			}
			Util::set_non_blocking(connection->fd);
			if(!this->host_addresses.count(connection->hostname)){
				struct hostent* host;
				if((host = gethostbyname(connection->hostname.c_str())) == 0){
					ERROR("gethostbyname")
					connection->state = DEAD;
					break;
				}
				this->host_addresses[connection->hostname] = *reinterpret_cast<struct in_addr*>(host->h_addr);
			}
			connection->server_address->sin_family = AF_INET;
			connection->server_address->sin_addr = this->host_addresses[connection->hostname];
			connection->server_address->sin_port = htons(connection->port);
			running = true;
			break;
		case CONNECTING:
			bzero(&(connection->server_address->sin_zero), 8);
			if(connect(connection->fd, reinterpret_cast<struct sockaddr*>(connection->server_address), sizeof(struct sockaddr_in)) < 0){
				if(!(errno == EALREADY || errno == EINPROGRESS)){
					ERROR("connect")
					connection->state = DEAD;
					break;
				}
				// Still connecting, cooooool.
			}else{
				connection->state = CONNECTED;
				PRINT(connection->fd << " connected!")
				if(this->on_connect != nullptr){
					if(this->on_connect(connection->fd)){
						connection->state = DEAD;
					}
				}
			}
			running = true;
			break;
		case CONNECTED:
			ssize_t len;
			if((len = read(connection->fd, packet, PACKET_LIMIT)) < 0){
				if(errno != EWOULDBLOCK){
					ERROR("read")
					connection->state = DEAD;
					break;
				}
				// Nothing to read, coooooool.
			}else if(len == 0){
				ERROR("server read zero")
				connection->state = DEAD;
				break;
			}else{
				packet[len] = 0;
				if(this->on_read(connection->fd, packet, len)){
					connection->state = DEAD;
				}
			}
			running = true;
			break;
		case DEAD:
			break;
		}
	}
	}
}
