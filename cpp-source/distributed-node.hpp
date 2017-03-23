#pragma once

#include "json.hpp"
#include "symmetric-tcp-client.hpp"
#include "symmetric-epoll-server.hpp"

class DistributedNode{
public:
	DistributedNode(std::string keyfile, const char* start_ip_address, uint16_t start_port);
	bool set_ddata(std::string data);
	
	JsonObject status;
	JsonObject ddata;
private:
	SymmetricEpollServer* server;
	
	std::mutex clients_mutex;
	std::unordered_map<std::string, SymmetricTcpClient*> clients;
	
	std::thread start_thread;
	
	[[noreturn]] void start();
};
