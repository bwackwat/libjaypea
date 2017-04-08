#pragma once

#include <mutex>

#include "simple-tcp-client.hpp"
#include "symmetric-encryptor.hpp"

class SymmetricTcpClient : public SimpleTcpClient{
public:
	SymmetricTcpClient(std::string hostname, uint16_t port, std::string keyfile);
	SymmetricTcpClient(const char* ip_address, uint16_t port, std::string keyfile);

	std::string communicate(std::string request);
	std::string communicate(const char* request, size_t length);
private:
	SymmetricEncryptor encryptor;
	int writes;
	int reads;
	int retries;
	
	std::mutex comm_mutex;
	
	void close_client();
};
