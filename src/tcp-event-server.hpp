#pragma once

#include <stack>
#include <vector>
#include <mutex>
#include <functional>

#include "queue.hpp"

enum EventType{
	BROADCAST
};

class Event{
public:
	enum EventType type;
	const char* data;
	size_t data_length;
	Event(enum EventType new_type, const char* new_data, size_t new_data_length)
	:type(new_type), data(new_data), data_length(new_data_length){}
};

class EventServer{
protected:
	std::string name;
	unsigned long max_connections;
	unsigned int num_threads;
	bool running;

	Queue<Event*>* thread_event_queues;
	std::mutex* thread_event_queue_mutexes;

	int server_fd;
	std::atomic<unsigned int> accepting_id;
	
	virtual bool non_blocking_accept(int* new_client_fd);
	virtual void close_client(size_t index, std::function<void(size_t)> callback);
public:
	EventServer(std::string new_name, uint16_t port, size_t new_max_connections);
	virtual ~EventServer(){}

	virtual bool send(int fd, const char* data, size_t data_length);
	virtual bool recv(int fd, char* data, size_t data_length);

	virtual void run(bool returning);
	void run_thread(unsigned int id);

	void start_event(Event* event);

	std::function<void(int)> on_connect;
	std::function<bool(int, const char*, ssize_t)> on_read;
	std::function<void(int)> on_disconnect;
};
