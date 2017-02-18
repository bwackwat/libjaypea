#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <utility>
#include <fstream>
#include <cstring>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "util.hpp"
#include "json.hpp"
#include "tcp-server.hpp"
#include "tls-epoll-server.hpp"

#define BUFFER_LIMIT 8192
#define HTTP_404 "<h1>404 Not Found</h1>"

class Route{
public:
	std::function<std::string(JsonObject*)> function;
	std::unordered_map<std::string, JsonType> requires;
	bool requires_human;

	Route(std::function<std::string(JsonObject*)> new_function, std::unordered_map<std::string, JsonType> new_requires, bool new_requires_human)
	:function(new_function), requires(new_requires), requires_human(new_requires_human)
	{}
};

class CachedFile{
public:
	char* data;
	size_t data_length;
};

class HttpApi{
public:
	JsonObject* routes_object;
	std::string routes_string;

	HttpApi(std::string new_public_directory, EpollServer* new_server);
	void route(std::string method, std::string path, std::function<std::string(JsonObject*)> function, std::unordered_map<std::string, JsonType> requires = std::unordered_map<std::string, JsonType>(), bool requires_human = false);
	void start();
	void set_file_cache_size(int megabytes);
private:
	std::unordered_map<std::string, CachedFile*> file_cache;
	int file_cache_remaining_bytes;
	std::mutex file_cache_mutex;

	std::unordered_map<std::string, Route*> routemap;
	std::string public_directory;

	EpollServer* server;
};
