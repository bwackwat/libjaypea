#pragma once

#include <stdexcept>
#include <string>
#include <cstring>
#include <iostream>
#include <csignal>
#include <typeinfo>
#include <atomic>
#include <cstdio>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "json.hpp"

#include "openssl/ssl.h"
#include "openssl/err.h"

class TlsClientManager{
protected:
	std::unordered_map<std::string, struct in_addr> host_addresses;
	std::string name;
	bool verbose;

	bool send(SSL* ssl, const char* data, size_t data_length);
	ssize_t recv(SSL* ssl, char* data, size_t data_length);
public:
	TlsClientManager(bool new_verbose = false);

	bool get(uint16_t port, std::string hostname, std::string path, char* response);
	bool post(uint16_t port, std::string hostname, std::string path, std::unordered_map<std::string, std::string> headers, std::string body, char* response);
	bool communicate(std::string hostname, uint16_t port, const char* request, size_t length, char* response);
	~TlsClientManager();
};
