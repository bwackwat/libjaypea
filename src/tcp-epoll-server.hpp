#pragma once

#include <stack>
#include <vector>
#include <mutex>
#include <functional>
#include <thread>
#include <unordered_map>

#include "sys/epoll.h"

#include "tcp-event-server.hpp"

class EpollServer : public EventServer{
protected:
	int epoll_fd;
	struct epoll_event new_event;
	struct epoll_event* client_events;
	
	virtual void run_thread(unsigned int thread_id);
public:
	EpollServer(uint16_t port, size_t new_max_connections, std::string new_name = "EpollServer");
	virtual ~EpollServer(){}
};
