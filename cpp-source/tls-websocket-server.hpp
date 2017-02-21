#pragma once

#include <functional>

#include "util.hpp"
#include "tcp-server.hpp"
#include "tls-epoll-server.hpp"
#include "websocket.hpp"

class TlsWebsocketServer : public TlsEpollServer{
public:
	TlsWebsocketServer(std::string certificate, std::string private_key, uint16_t port, size_t max_connections);

	bool send(int fd, const char* data, size_t data_length);
	ssize_t recv(int fd, char* data, size_t data_length);
private:
	Websocket websocket;

	bool accept_continuation(int* fd);
};
