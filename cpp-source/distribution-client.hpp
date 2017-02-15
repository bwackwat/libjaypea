#pragma once

#include "util.hpp"
#include "symmetric-tcp-client.hpp"

class DistributionClient{
public:
	DistributionClient(std::string new_keyfile, std::string new_pasword);

	std::string get();
private:
	std::string keyfile;
	std::string password;
	uint16_t port;

	std::unordered_map<std::string, SymmetricTcpClient*> hosts;
};
