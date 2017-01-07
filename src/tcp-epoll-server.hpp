#pragma once

#include <stack>
#include <vector>
#include <mutex>
#include <functional>
#include <thread>
#include <unordered_map>

#include "sys/epoll.h"

class EpollServer{
protected:
	unsigned long max_connections;
	unsigned int num_threads;
	std::string name;

	int server_fd;
	int epoll_fd;
	struct epoll_event new_event;
	struct epoll_event* client_events;
	
	void run_thread(unsigned int thread_id);

	//virtual bool accept_continuation(int* new_client_fd);
public:
	EpollServer(uint16_t port, size_t new_max_connections, std::string new_name = "EpollServer");
	virtual ~EpollServer(){}

	virtual bool send(int fd, const char* data, size_t data_length);
	virtual ssize_t recv(int fd, char* data, size_t data_length);

	void run(bool returning = false, unsigned int new_num_threads = std::thread::hardware_concurrency());

	std::function<void(int)> on_connect;
	std::function<ssize_t(int, const char*, size_t)> on_read;
	std::function<void(int)> on_disconnect;

	void start_event();
};
