#include <string>
#include <iostream>
#include <cstring>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ponal.hpp"

Ponal::Ponal(uint16_t port, std::string hostname)
	:port(port),
	hostname(hostname){
	if(this->Reset()){
		std::cout << "Ponal has failed.\n";
	}
}

int Ponal::Reset(uint16_t port, std::string hostname){
	struct hostent *host;	
	struct sockaddr_in serv_addr;
	int res;

	if(!hostname.empty()){
		this->hostname = hostname;
	}
	if(port){
		this->port = port;
	}

	if((this->socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		std::cout << "System call to socket failed!\n" << socketfd;
		return (this->disconnected = 1);
	}

	if((host = gethostbyname(this->hostname.c_str())) == 0){
		std::cout << "System call to gethostbyname failed!\n";
		std::cout << this->hostname << std::endl;
		return (this->disconnected = 1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = *reinterpret_cast<struct in_addr *>(host->h_addr);
	serv_addr.sin_port = htons(this->port);
	bzero(&(serv_addr.sin_zero), 8);

	if((res = connect(this->socketfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr))) < 0){
		std::cout << "System call to connect failed!\n" << res << errno;
		return (this->disconnected = 1);
	}

	return (this->disconnected = 0);
}

std::string Ponal::Communicate(const char* request, size_t len){
	char response[PACKET_LIMIT];
	ssize_t slen;

	if(write(this->socketfd, request, len) < 0){
		std::cout << "System call to write failed!\n";
                this->disconnected = 1;
		return "";
	}

	if((slen = read(this->socketfd, response, PACKET_LIMIT)) <= 0){
		std::cout << "Ponal call to read was bad!\n";
		this->disconnected = 1;
		return "";
	}
	response[slen] = 0;

	return std::string(response);
}

std::string Ponal::Get(std::string key){
	std::string get = "get " + key;
	return this->Communicate(get.c_str(), get.length());
}

int Ponal::Set(std::string key, std::string value){
	std::string set = "set " + key + ' ' + value;
	return std::strcmp(this->Communicate(set.c_str(), set.length()).c_str(), FAILURE);
}
