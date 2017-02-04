#pragma once

#include <mutex>

#include "util.hpp"
#include "simple-tcp-client.hpp"
#include "symmetric-encryptor.hpp"

class SymmetricTcpClient : public SimpleTcpClient{
public:
	SymmetricTcpClient(std::string ip_address, uint16_t port, std::string keyfile);

	bool reconnect();
	std::string communicate(std::string request);
	std::string communicate(const char* request, size_t length);
private:
	std::mutex comm_mutex;
	int writes;
	int reads;
	SymmetricEncryptor encryptor;
};
