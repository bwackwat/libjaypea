#pragma once

#include "simple-tcp-client.hpp"
#include "symmetric-encryptor.hpp"

class SymmetricTcpClient : public SimpleTcpClient{
public:
	SymmetricTcpClient(std::string ip_address, uint16_t port, std::string keyfile);

	void reconnect();
	bool communicate(const char* request, size_t length, char* response);
private:
	int writes;
	int reads;
	SymmetricEncryptor encryptor;
};
