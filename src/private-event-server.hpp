#pragma once

#include <unordered_map>

#include "openssl/ssl.h"
#include "openssl/err.h"

class PrivateEventServer : public EventServer{
private:
	SSL_CTX* ctx;
	std::unordered_map<int, SSL*> client_ssl;

	void close_client(size_t index, int* fd, std::function<void(size_t, int*)> callback);
	ssize_t recv(int fd, char* data, size_t data_length);
	bool accept_continuation(int* new_client_fd);
public:
	PrivateEventServer(std::string certificate, std::string private_key, uint16_t port, size_t max_connections);
	~PrivateEventServer();
	bool send(int fd, const char* data, size_t data_length);
};
