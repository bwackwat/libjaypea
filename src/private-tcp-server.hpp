#pragma once

#include <unordered_map>

#include "openssl/ssl.h"
#include "openssl/err.h"

#include "tcp-server.hpp"

class PrivateEpollServer : public EpollServer{
private:
	SSL_CTX* ctx;
	std::unordered_map<int, SSL*> client_ssl;

	void close_client(int* fd, std::function<void(int*)> callback);
	ssize_t recv(int fd, char* data, size_t data_length);
	bool accept_continuation(int* new_client_fd);
public:
	PrivateEpollServer(std::string certificate, std::string private_key, uint16_t port, size_t max_connections);
	~PrivateEpollServer();
	ssize_t recv(int fd, char* data, size_t data_length, std::function<ssize_t(int, char*, size_t)> callback);
	bool send(int fd, const char* data, size_t data_length);
};
