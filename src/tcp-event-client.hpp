#pragma once

#include <unordered_map>

enum ConnectionState{
	DISCONNECTED,
	CONNECTING,
	CONNECTED,
	DEAD
};

struct Connection{
	int fd;
	std::string hostname;
	uint16_t port;
	enum ConnectionState state;
	struct sockaddr_in* server_address;
};

class EventClient{
private:
	std::unordered_map<std::string, struct in_addr> host_addresses;
	std::vector<struct Connection*> connections;

	std::function<bool(int, const char*, ssize_t)> on_read;
public:
	void add(std::string hostname, uint16_t port);
	void run(std::function<bool(int, const char*, ssize_t)> new_on_read);

	std::function<bool(int)> on_connect;
};
