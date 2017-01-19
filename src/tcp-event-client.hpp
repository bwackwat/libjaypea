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
	std::string ip_address;
	uint16_t port;
	enum ConnectionState state;
	struct sockaddr_in* server_address;

	Connection(std::string new_hostname, uint16_t new_port)
	:fd(-1), hostname(new_hostname), port(new_port), state(DISCONNECTED),
	server_address(new struct sockaddr_in()){}

	Connection(uint16_t new_port, std::string new_ip_address)
	:fd(-1), hostname(std::string()), ip_address(new_ip_address), port(new_port), state(DISCONNECTED),
	server_address(new struct sockaddr_in()){}
};

class EventClient{
protected:
	std::unordered_map<std::string, struct in_addr> host_addresses;
	std::vector<Connection*> connections;
	int alive_connections;

	std::unordered_map<int /* fd */, int> read_counter;
	std::unordered_map<int /* fd */, int> write_counter;

	std::function<ssize_t(int, const char*, ssize_t)> on_read;

	void close_client(Connection* conn);
	virtual bool send(int fd, const char* data, size_t data_length);
	virtual ssize_t recv(int fd, char* data, size_t data_length);
public:
	virtual ~EventClient(){}

	void add(Connection* conn);
	void run(std::function<ssize_t(int, const char*, size_t)> new_on_read);

	std::function<bool(int)> on_connect;
	std::function<bool()> on_event_loop;
};
