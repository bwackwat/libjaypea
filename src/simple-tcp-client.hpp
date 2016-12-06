#pragma once

#include <string>

#include <sys/types.h>

class SimpleTcpClient{
private:
	std::string hostname;
	uint16_t port;
	std::string name;
public:
	SimpleTcpClient(std::string new_hostname, uint16_t new_port);
	void communicate(const char* request, size_t length, char* response);
	~SimpleTcpClient();

	int fd;
};
