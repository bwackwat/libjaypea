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

	this->connected = this->reconnect();
}
/*
void connect_w_to(void) { 
	int res; 
	struct sockaddr_in addr; 
	long arg; 
	fd_set myset; 
	struct timeval tv; 
	socklen_t lon;

	// Create socket 
	soc = socket(AF_INET, SOCK_STREAM, 0); 
	if (soc < 0) {
		fprintf(stderr, "Error creating socket (%d %s)\n", errno, strerror(errno));
		exit(0);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(2000);
	addr.sin_addr.s_addr = inet_addr("192.168.0.1");

	// Set non-blocking
	if( (arg = fcntl(soc, F_GETFL, NULL)) < 0) {
		fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
		exit(0);
	}
	arg |= O_NONBLOCK;
	if(fcntl(soc, F_SETFL, arg) < 0){
		fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
		exit(0);
	}
	// Trying to connect with timeout
	res = connect(soc, (struct sockaddr *)&addr, sizeof(addr));
	if(res < 0){
		if(errno == EINPROGRESS){
			fprintf(stderr, "EINPROGRESS in connect() - selecting\n");
			do{
				tv.tv_sec = 15;
				tv.tv_usec = 0;
				FD_ZERO(&myset);
				FD_SET(soc, &myset);
				res = select(soc+1, NULL, &myset, NULL, &tv);
				if(res < 0 && errno != EINTR){
					fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
					exit(0);
				}else if(res > 0){ 
					// Socket selected for write 
					lon = sizeof(int); 
					if(getsockopt(soc, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0){ 
						fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno)); 
						exit(0); 
					} 
					// Check the value returned... 
					if(valopt){ 
						fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt, strerror(valopt) 
						); 
						exit(0); 
					} 
					break; 
				}else{
					fprintf(stderr, "Timeout in select() - Cancelling!\n");
					exit(0);
				}
			}while(1);
		}else{
			fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
			exit(0);
		}
	}
	// Set to blocking mode again...
	if( (arg = fcntl(soc, F_GETFL, NULL)) < 0) {
	fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
	exit(0);
	}
	arg &= (~O_NONBLOCK);
	if( fcntl(soc, F_SETFL, arg) < 0) {
	fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
	exit(0);
	}
	// I hope that is all
}
*/
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
			perror("reconnect timeout");
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
