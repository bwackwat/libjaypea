#pragma once

#include <stack>
#include <vector>
#include <functional>

#include "node.hpp"

typedef std::function<bool(int, const char*, ssize_t)> PacketReceivedFunction;

class EventServer{
protected:
	std::string name;
	unsigned long max_connections;
	Node<size_t>* next_fds;

	int server_fd;
	
	std::vector<int> client_fds;

	virtual bool nonblocking_accept();
	virtual void close_client(size_t index);
public:
	EventServer(std::string new_name, uint16_t port, size_t new_max_connections);
	virtual ~EventServer(){}

	virtual void broadcast(const char* packet, size_t length);
	virtual bool recv(int fd, char* data, size_t data_length);
	virtual void run(PacketReceivedFunction new_packet_received);

	std::function<void(int)> on_connect;
	std::function<bool()> on_event_loop;
	PacketReceivedFunction packet_received;
	std::function<void(int)> on_disconnect;
};
