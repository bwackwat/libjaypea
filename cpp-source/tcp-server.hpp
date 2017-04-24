#pragma once

#include <stack>
#include <vector>
#include <mutex>
#include <ctime>
#include <functional>
#include <thread>
#include <unordered_map>

#include "signal.h"
#include "sys/timerfd.h"
#include "sys/epoll.h"
#include <arpa/inet.h>

#include "queue.hpp"

#define EVENTS EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLHUP

struct ClientDetails{
	std::chrono::milliseconds ms_at_recv;
	std::chrono::milliseconds ms_diff_at_last;
};

class EpollServer{
protected:
	std::string name;
	unsigned long max_connections;
	unsigned int num_threads;
	time_t timeout;
	socklen_t sockaddr_length;

	std::unordered_map<int /* fd */, int> read_counter;
	std::unordered_map<int /* fd */, int> write_counter;
	//uint64_t represents millseconds since last recv
	std::mutex client_time_mutex;

	int server_fd;
	std::mutex accept_mutex;
	struct sockaddr_in accept_client;
	
	// Read from 0, write to 1.
	int broadcast_pipe[2];

	virtual void run_thread(unsigned int id);
	virtual bool accept_continuation(int* new_client_fd);
	virtual void close_client(int* fd, std::function<void(int*)> callback);
public:
	EpollServer(uint16_t port, size_t new_max_connections, std::string new_name = "EpollServer");
	virtual ~EpollServer(){}

	std::unordered_map<int /* client fd */, std::string> fd_to_details_map;
	bool running;

	void set_timeout(time_t seconds);
	int broadcast_fd();

	bool send(int fd, std::string data);
	virtual bool send(int fd, const char* data, size_t data_length);
	virtual ssize_t recv(int fd, char* data, size_t data_length);
	virtual ssize_t recv(int fd, char* data, size_t data_length, std::function<ssize_t(int, char*, size_t)> callback);

	virtual void run(bool returning = false, unsigned int new_num_threads = std::thread::hardware_concurrency());

	/// When a connection has successfully been made, this is called.
	std::function<void(int)> on_connect;

	/// When a connection has read some data, this is called.
	std::function<ssize_t(int, const char*, size_t)> on_read;

	/// When a previously successfully made connection has been closed, this is called.
	std::function<void(int)> on_disconnect;
};
