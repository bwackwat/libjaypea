#pragma once

#include <stack>
#include <vector>

#include "node.hpp"

class EventServer{
private:
	std::string name;
	unsigned long max_connections;
	Node<size_t>* next_fds;

	int server_fd;
	
	std::vector<int> client_fds;
public:
	EventServer(uint16_t port, size_t new_max_connections);
	void run();
};
