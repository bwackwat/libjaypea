#include "util.hpp"
#include "simple-tcp-client.hpp"

SimpleTcpClient::SimpleTcpClient(std::string hostname, uint16_t new_port, bool new_verbose)
:port(new_port),
name("SimpleTcpClient"),
verbose(new_verbose),
requests(0){
	struct hostent* host;
	if((host = gethostbyname(hostname.c_str())) == 0){
		perror("Could not get host by name.");
		this->connected = false;
		return;
	}
	this->remote_address = *reinterpret_cast<struct in_addr*>(host->h_addr);

	this->connected = this->reconnect();
}

SimpleTcpClient::SimpleTcpClient(struct in_addr new_remote_address, uint16_t new_port, bool new_verbose)
:port(new_port),
name("SimpleTcpClient"),
verbose(new_verbose),
remote_address(new_remote_address),
requests(0){
	this->connected = this->reconnect();
}

SimpleTcpClient::SimpleTcpClient(const char* ip_address, uint16_t new_port, bool new_verbose)
:port(new_port),
name("SimpleTcpClient"),
verbose(new_verbose),
requests(0){
	this->remote_address.s_addr = inet_addr(ip_address);

	this->connected = this->reconnect();
}

bool SimpleTcpClient::reconnect(){
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr = this->remote_address;
	server_addr.sin_port = htons(this->port);
	bzero(&(server_addr.sin_zero), 8);
	
	if((this->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("reconnect socket");
		return false;
	}
	
	Util::set_non_blocking(this->fd);
	
	if(connect(this->fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(struct sockaddr_in)) < 0){
		if(errno != EINPROGRESS){
			perror("reconnect connect");
			return false;
		}
		
		fd_set myset;
		struct timeval tv;
		socklen_t lon = sizeof(int);
		int res;
		int valopt;
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_ZERO(&myset);
		FD_SET(this->fd, &myset);
		if((res = select(this->fd + 1, NULL, &myset, NULL, &tv)) < 0){
			perror("error in select");
			return false;
		}else if(res > 0){
			if(getsockopt(this->fd, SOL_SOCKET, SO_ERROR, &valopt, &lon) < 0){
				perror("getsockopt reconnect");
				return false;
			}
			if(valopt){
				perror("error in delayed reconnect");
				return false;
			}
			// No errors, successful connect.
		}else{
			return false;
		}
	}
	
	if(this->verbose){
		PRINT(this->name << " connected")
	}
	
	Util::set_blocking(this->fd);
	
	return true;
}

bool SimpleTcpClient::communicate(const char* request, size_t length, char* response){
	while(!this->connected){
		this->reconnect();
	}
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
