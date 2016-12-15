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
	PacketReceivedFunction packet_received;

	int server_fd;
	
	std::vector<int> client_fds;
public:
	EventServer(uint16_t port, size_t new_max_connections, PacketReceivedFunction new_packet_received);
	void run();
};
