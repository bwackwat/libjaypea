#pragma once

#include <stack>
#include <vector>
#include <functional>

#include "node.hpp"

typedef std::function<bool(int, char*, ssize_t)> PacketReceivedFunction;

class EventServer{
private:
	std::string name;
	unsigned long max_connections;
	Node<size_t>* next_fds;

	int server_fd;
	
	std::vector<int> client_fds;

	void close_client(size_t index);
public:
	EventServer(uint16_t port, size_t new_max_connections);
	void broadcast(const char* packet, size_t length);
	void run(PacketReceivedFunction new_packet_received);

	std::function<void(int)> on_connect;
	std::function<void(int)> on_disconnect;
};
