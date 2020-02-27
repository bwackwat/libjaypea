#include <stdexcept>

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.hpp"
#include "simple-tcp-server.hpp"

SimpleTcpServer::SimpleTcpServer(uint16_t port, int backlog)
:name("SimpleTcpServer"){
	if((this->server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		throw std::runtime_error(this->name + " socket");
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	if(bind(this->server_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0){
		throw std::runtime_error(this->name + " bind");
	}
	if(listen(this->server_fd, backlog) < 0){
		throw std::runtime_error(this->name + " listen");
	}
	this->sockaddr_length = sizeof(this->client_addr);
	this->hit = 1;
	PRINT(this->name << " listening on " << port)
}

int SimpleTcpServer::accept_connection(){
	int client_fd;
	if((client_fd = accept(this->server_fd, reinterpret_cast<struct sockaddr*>(&this->client_addr), &this->sockaddr_length)) < 0){
		throw std::runtime_error(this->name + " accept");
	}
	PRINT(this->name << " accepted connection #" << this->hit)
	this->hit++;
	return client_fd;
}

SimpleTcpServer::~SimpleTcpServer(){
	if(close(this->server_fd) < 0){
		perror("close");
	}
}
