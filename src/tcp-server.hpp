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

class EpollServer{
protected:
	std::string name;
	unsigned long max_connections;
	unsigned int num_threads;
	bool running;
	time_t timeout;

	std::unordered_map<int /* fd */, int> read_counter;
	std::unordered_map<int /* fd */, int> write_counter;

	int server_fd;
	std::mutex accept_mutex;
	// Read from 0, write to 1.
	int broadcast_pipe[2];
	
	/// Reference to working epoll code. Unused (because timeouts are essential.)
	/// virtual void run_thread_no_timeouts(unsigned int thread_id);
	virtual void run_thread(unsigned int id);
	virtual bool accept_continuation(int* new_client_fd);
	virtual void close_client(int* fd, std::function<void(int*)> callback);
public:
	EpollServer(uint16_t port, size_t new_max_connections, std::string new_name = "EpollServer");
	virtual ~EpollServer(){}

	void set_timeout(time_t seconds);
	int broadcast_fd();

	virtual bool send(int fd, const char* data, size_t data_length);
	virtual ssize_t recv(int fd, char* data, size_t data_length);

	virtual void run(bool returning = false, unsigned int new_num_threads = std::thread::hardware_concurrency());

	/// When a connection has successfully been made, this is called.
	std::function<void(int)> on_connect;

	/// When a connection has read some data, this is called.
	std::function<ssize_t(int, const char*, ssize_t)> on_read;

	/// When a previously successfully made connection has been closed, this is called.
	std::function<void(int)> on_disconnect;
};
