#pragma once

#include "util.hpp"

class WebsocketServer: public EpollServer{
public:
	WebsocketServer(uint16_t port, size_t max_connections, std::string name);
	bool send(int fd, const char* data, size_t data_length);
	ssize_t recv(int fd, char* data, size_t data_length);
private:
	std::unordered_map<int, bool> client_handshaked;
	bool accept_continuation(int* fd);
};
