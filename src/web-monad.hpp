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
#include "tcp-event-server.hpp"
#include "private-event-server.hpp"

#define BUFFER_LIMIT 8192
#define HTTP_404 "<h1>404 Not Found</h1>"

enum RequestResult {
	HTTP,
	HTTP_API,
	API
};

class Route{
public:
	std::function<std::string(JsonObject*)> function;
	std::unordered_map<std::string, JsonType> requires;
	bool requires_human;

	Route(std::function<std::string(JsonObject*)> new_function, std::unordered_map<std::string, JsonType> new_requires, bool new_requires_human)
	:function(new_function), requires(new_requires), requires_human(new_requires_human)
	{}
};

class WebMonad{
public:
	JsonObject* routes_object;
	std::string routes_string;

	WebMonad(std::string hostname, std::string new_public_directory, std::string ssl_certificate, std::string ssl_private_key, uint16_t http_port = 80, uint16_t https_port = 443);
	void route(std::string method, std::string path, std::function<std::string(JsonObject*)> function, std::unordered_map<std::string, JsonType> requires = std::unordered_map<std::string, JsonType>(), bool requires_human = false);
	void start();
private:
	std::unordered_map<std::string, Route*> routemap;
	std::string public_directory;
	EpollServer* redirecter;
	PrivateEventServer* server;

	const char* http_response;
	size_t http_response_length;

	enum RequestResult parse_request(const char* request, JsonObject* request_obj);
};
