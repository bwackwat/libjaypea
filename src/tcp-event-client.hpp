#pragma once

#include <unordered_map>

enum ConnectionState{
	DISCONNECTED,
	CONNECTING,
	CONNECTED,
	DEAD
};

class Connection{
public:
	int fd;
	std::string hostname;
	uint16_t port;
	enum ConnectionState state;
	struct sockaddr_in* server_address;

	Connection(std::string new_hostname, uint16_t new_port)
	:fd(-1),
	hostname(new_hostname),
	port(new_port),
	state(DISCONNECTED),
	server_address(new struct sockaddr_in()){}
};

class EventClient{
private:
	std::unordered_map<std::string, struct in_addr> host_addresses;
	std::vector<Connection*> connections;

	std::function<bool(int, const char*, ssize_t)> on_read;

	void close_client(Connection* conn);
public:
	void add(std::string hostname, uint16_t port);
	void run(std::function<bool(int, const char*, ssize_t)> new_on_read);

	std::function<bool(int)> on_connect;
};
