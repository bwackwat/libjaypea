#pragma once

#include "json.hpp"
#include "symmetric-tcp-client.hpp"
#include "symmetric-epoll-server.hpp"

class DistributedNode{
public:
	DistributedNode(std::string keyfile);
	bool set_ddata(std::string data);
	void add_client(const char* ip_address, uint16_t port);
	
	JsonObject status;
	JsonObject ddata;
private:
	SymmetricEpollServer* server;
	std::string keyfile;
	
	std::mutex clients_mutex;
	std::unordered_map<std::string, SymmetricTcpClient*> clients;
	
	std::thread start_thread;
	
	[[noreturn]] void start();
};
