#include <stdexcept>
#include <string>
#include <cstring>
#include <iostream>
#include <csignal>

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
#include "simple-tcp-client.hpp"

SimpleTcpClient::SimpleTcpClient(std::string new_hostname, uint16_t new_port)
:hostname(new_hostname),
port(new_port),
name("SimpleTcpClient"){
	if((this->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		throw std::runtime_error(this->name + " socket");
	}
	struct hostent* host;
	if((host = gethostbyname(hostname.c_str())) == 0){
		throw std::runtime_error(this->name + " gethostbyname");
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr = *reinterpret_cast<struct in_addr*>(host->h_addr);
	server_addr.sin_port = htons(this->port);
	bzero(&(server_addr.sin_zero), 8);
	if(connect(this->fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0){
		throw std::runtime_error(this->name + " connect");
	}
	PRINT(this->name << " connected on " << this->port)
}

void SimpleTcpClient::communicate(const char* request, size_t length, char* response){
	ssize_t len;
	if(write(this->fd, request, length) < 0){
		throw std::runtime_error(this->name + " write");
	}
	if((len = read(this->fd, response, PACKET_LIMIT)) < 0){
		throw std::runtime_error(this->name + " read");
	}
	response[len] = 0;
}

SimpleTcpClient::~SimpleTcpClient(){
	if(close(this->fd) < 0){
		throw std::runtime_error(this->name + " close");
	}
}
