#pragma once

#include <functional>

#include "util.hpp"
#include "tcp-server.hpp"
#include "websocket.hpp"

class WebsocketServer : public EpollServer{
public:
	WebsocketServer(uint16_t port, size_t max_connections);

	bool send(int fd, const char* data, size_t data_length);
	ssize_t recv(int fd, char* data, size_t data_length);
private:
	Websocket websocket;

	bool accept_continuation(int* fd);
};
