#pragma once

#include <unordered_map>

#include "openssl/ssl.h"
#include "openssl/err.h"

#include "tcp-server.hpp"

class TlsEpollServer : public EpollServer{
private:
	SSL_CTX* ctx;
	std::unordered_map<int, SSL*> client_ssl;

	void close_client(int* fd, std::function<void(int*)> callback);
public:
	TlsEpollServer(std::string certificate, std::string private_key, uint16_t port, size_t max_connections, std::string new_name = "TlsEpollServer");
	~TlsEpollServer();

	virtual bool accept_continuation(int* new_client_fd);
	virtual bool send(int fd, const char* data, size_t data_length);
	virtual ssize_t recv(int fd, char* data, size_t data_length);
	virtual ssize_t recv(int fd, char* data, size_t data_length, std::function<ssize_t(int, char*, size_t)> callback);
};
