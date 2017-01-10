#pragma once

#include <stack>
#include <vector>
#include <mutex>
#include <functional>
#include <thread>
#include <unordered_map>
#include <atomic>

#include "sys/epoll.h"

#include "tcp-event-server.hpp"

class EpollServer : public EventServer{
protected:
	int epoll_fd;
	struct epoll_event new_event;
	struct epoll_event* client_events;

	std::atomic<unsigned long> num_connections;
	std::unordered_map<int /* fd */, std::mutex> fd_mutexes;
	
	virtual void run_thread(unsigned int thread_id);
public:
	EpollServer(uint16_t port, size_t new_max_connections, std::string new_name = "EpollServer");
	virtual ~EpollServer(){}

	virtual ssize_t recv(int fd, char* data, size_t data_length);
};
