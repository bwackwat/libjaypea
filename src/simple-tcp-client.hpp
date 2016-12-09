#pragma once

#include <string>

#include <sys/types.h>

class SimpleTcpClient{
private:
	std::string hostname;
	uint16_t port;
	std::string name;
	bool verbose;
public:
	SimpleTcpClient(std::string new_hostname, uint16_t new_port, bool new_verbose = false);
	SimpleTcpClient(uint16_t new_port, struct in_addr addr, bool new_verbose = false);
	bool communicate(const char* request, size_t length, char* response);
	~SimpleTcpClient();

	int fd;
};
