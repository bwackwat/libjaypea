#include "util.hpp"
#include "simple-tcp-client.hpp"

SimpleTcpClient::SimpleTcpClient(std::string hostname, uint16_t new_port, bool new_verbose)
:port(new_port),
name("SimpleTcpClient"),
verbose(new_verbose),
requests(0){
	struct hostent* host;
	if((host = gethostbyname(hostname.c_str())) == 0){
		perror("ctor gethostbyname");
		throw std::runtime_error(this->name + " gethostbyname " + std::to_string(errno).c_str());
	}
	this->remote_address = *reinterpret_cast<struct in_addr*>(host->h_addr);

	this->reconnect();
}

SimpleTcpClient::SimpleTcpClient(struct in_addr new_remote_address, uint16_t new_port, bool new_verbose)
:port(new_port),
name("SimpleTcpClient"),
verbose(new_verbose),
remote_address(new_remote_address),
requests(0){
	this->reconnect();
}

SimpleTcpClient::SimpleTcpClient(const char* ip_address, uint16_t new_port, bool new_verbose)
:port(new_port),
name("SimpleTcpClient"),
verbose(new_verbose),
requests(0){
	this->remote_address.s_addr = inet_addr(ip_address);

	this->reconnect();
}

void SimpleTcpClient::reconnect(){
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr = this->remote_address;
	server_addr.sin_port = htons(this->port);
	bzero(&(server_addr.sin_zero), 8);
	if((this->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("reconnect socket");
		throw std::runtime_error(this->name + " socket " + std::to_string(errno).c_str());
	}
	if(connect(this->fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(struct sockaddr_in)) < 0){
		perror("reconnect connect");
		throw std::runtime_error(this->name + " connect " + std::to_string(errno).c_str());
	}
	if(this->verbose){
		PRINT(this->name << " connected")
	}
}

bool SimpleTcpClient::communicate(const char* request, size_t length, char* response){
	ssize_t len;
	if(write(this->fd, request, length) < 0){
		perror("write");
		throw std::runtime_error(this->name + " write");
	}
	if((len = read(this->fd, response, PACKET_LIMIT)) < 0){
		perror("read");
		throw std::runtime_error(this->name + " read");
	}else if(len == 0){
		PRINT(this->name << ' ' << fd << " kicked from server after " << requests)
	}
	requests++;
	response[len] = 0;
	return len > 0;
}

SimpleTcpClient::~SimpleTcpClient(){
	if(this->verbose){
		PRINT(this->name + " delete")
	}
	if(close(this->fd) < 0){
		throw std::runtime_error(this->name + " close" + std::to_string(errno).c_str());
	}
}
