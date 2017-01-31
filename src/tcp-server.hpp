#pragma once

#include <stack>
#include <vector>
#include <mutex>
#include <functional>
#include <thread>
#include <unordered_map>

#include "signal.h"
#include "sys/timerfd.h"
#include "sys/epoll.h"

#include "queue.hpp"

#define EVENTS EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLHUP

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

class EpollServer{
protected:
	std::string name;
	unsigned long max_connections;
	unsigned int num_threads;
	bool running;

	std::unordered_map<int /* fd */, int> read_counter;
	std::unordered_map<int /* fd */, int> write_counter;

	Queue<Event*>* thread_event_queues;
	std::mutex* thread_event_queue_mutexes;

	int server_fd;
	std::mutex accept_mutex;
	
	virtual void run_thread(unsigned int id);
	virtual bool accept_continuation(int* new_client_fd);
	virtual void close_client(size_t index, int* fd, std::function<void(size_t, int*)> callback);
public:
	EpollServer(uint16_t port, size_t new_max_connections, std::string new_name = "EpollServer");
	virtual ~EpollServer(){}

	virtual bool send(int fd, const char* data, size_t data_length);
	virtual ssize_t recv(int fd, char* data, size_t data_length);

	virtual void run(bool returning = false, unsigned int new_num_threads = std::thread::hardware_concurrency());

	virtual void start_event(Event* event);

	/// When a connection has successfully been made, this is called.
	std::function<void(int)> on_connect;

	/// When a connection has read some data, this is called.
	std::function<ssize_t(int, const char*, ssize_t)> on_read;

	/// When a previously successfully made connection has been closed, this is called.
	std::function<void(int)> on_disconnect;
};
