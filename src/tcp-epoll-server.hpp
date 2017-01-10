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
	std::mutex accept_mutex;
	
	virtual void run_thread(unsigned int thread_id);
public:
	EpollServer(uint16_t port, size_t new_max_connections, std::string new_name = "EpollServer");
	virtual ~EpollServer(){}

	virtual ssize_t recv(int fd, char* data, size_t data_length);
};
