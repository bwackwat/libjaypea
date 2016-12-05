#pragma once

#include <string>

#define DEFAULT_HOSTNAME "localhost"
#define DEFAULT_PORT 4767
#define PACKET_LIMIT 2048
#define SUCCESS "success"
#define FAILURE "failure"

class Ponal{
public:
	Ponal(uint16_t port = DEFAULT_PORT, std::string hostname = DEFAULT_HOSTNAME);
	int Reset(uint16_t port = 0, std::string hostname = std::string());
	std::string Communicate(const char* request, size_t len);
	std::string Get(std::string key);
	int Set(std::string key, std::string value);

	int disconnected;
private:
	uint16_t port;
	std::string hostname;

	int socketfd;
};
