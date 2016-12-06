#pragma once

#include <string>

#include <sys/types.h>
#include <sys/socket.h>

class SimpleTcpServer{
private:
	std::string name;
	int server_fd;
	struct sockaddr_in client_addr;
	socklen_t sockaddr_length;
public:
	int hit;

	SimpleTcpServer(uint16_t port, int backlog = 10);
	~SimpleTcpServer();
	int accept_connection();
};
