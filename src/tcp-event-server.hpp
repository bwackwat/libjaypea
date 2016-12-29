#pragma once

#include <stack>
#include <vector>
#include <functional>

#include "stack-node.hpp"

enum EventType{
	BROADCAST
};

class Event{
public:
	enum EventType type;
	
	Event(enum EventType new_type):type(new_type){}
};

class EventServer{
protected:
	std::string name;
	unsigned long max_connections;

	

	int server_fd;
	
	virtual bool non_blocking_accept();
	virtual void close_client(size_t index);
public:
	EventServer(std::string new_name, uint16_t port, size_t new_max_connections);
	virtual ~EventServer(){}

	virtual void broadcast(const char* packet, size_t length);
	virtual bool recv(int fd, char* data, size_t data_length);

	virtual void run(bool returning);

	std::function<void(int)> on_connect;
	std::function<bool(int, const char*, ssize_t)> on_read;
	std::function<void(int)> on_disconnect;
};
