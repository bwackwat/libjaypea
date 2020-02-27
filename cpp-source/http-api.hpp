#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <utility>
#include <fstream>
#include <cstring>
#include <chrono>

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
#define INSUFFICIENT_ACCESS "{\"error\":\"Insufficient access.\"}"
#define NO_SUCH_ITEM "{\"error\":\"That record doesn't exist.\"}"

class HttpResponse{
public:
	HttpResponse(){}

	HttpResponse(std::string new_status, std::unordered_map<std::string, std::string> new_headers, std::string new_body)
	:status(new_status), headers(new_headers), body(new_body){}

	std::string start = "HTTP/1.1";
	std::string status = "200 Ok";
	std::unordered_map<std::string, std::string> headers = std::unordered_map<std::string, std::string>();
	std::string body = "";

	std::string str(){
		std::string response = start + " " + status + "\r\n";
		for(auto iter = this->headers.begin(); iter != this->headers.end(); ++iter){
			response += iter->first + ": " + iter->second + "\r\n";
		}
		response += "\r\n";
		return response + this->body;
	}
};

class View{
public:
	std::string route = "";
	std::string method = "";
	std::unordered_map<std::string, std::string> parameters = std::unordered_map<std::string, std::string>();
	ssize_t size;

	View(){}

	View(std::string new_route)
	:route(new_route){}
	
	View(std::string new_route, std::unordered_map<std::string, std::string> new_parameters)
	:route(new_route), parameters(new_parameters){}
};

class Route{
public:
	std::function<std::string(JsonObject*)> function;
	std::function<View(JsonObject*)> form_function;
	std::function<std::string(JsonObject*, JsonObject*)> token_function;

	std::unordered_map<std::string, JsonType> required_parameters;
	bool requires_human;

	std::chrono::milliseconds minimum_ms_between_call;
	std::unordered_map<std::string /* client identifier (IP) */, std::chrono::milliseconds> client_ms_at_call;

	Route(std::function<std::string(JsonObject*)> new_function,
		std::unordered_map<std::string, JsonType> new_required_parameters,
		std::chrono::milliseconds new_rate_limit,
		bool new_requires_human)
	:function(new_function),
	required_parameters(new_required_parameters),
	requires_human(new_requires_human),
	minimum_ms_between_call(new_rate_limit)
	{}

	Route(std::function<View(JsonObject*)> new_function,
		std::unordered_map<std::string, JsonType> new_required_parameters,
		std::chrono::milliseconds new_rate_limit,
		bool new_requires_human)
	:form_function(new_function),
	required_parameters(new_required_parameters),
	requires_human(new_requires_human),
	minimum_ms_between_call(new_rate_limit)
	{}

	Route(std::function<std::string(JsonObject*, JsonObject*)> new_function,
		std::unordered_map<std::string, JsonType> new_required_parameters,
		std::chrono::milliseconds new_rate_limit,
		bool new_requires_human)
	:token_function(new_function),
	required_parameters(new_required_parameters),
	requires_human(new_requires_human),
	minimum_ms_between_call(new_rate_limit)
	{}
};

class CachedFile{
public:
	char* data;
	size_t data_length;
	time_t modified;
};

class Session{
public:
	std::string captcha;
	std::chrono::seconds created = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch());
};

class HttpApi{
public:
	JsonObject* routes_object;
	std::string routes_string;

	HttpApi(std::string new_public_directory, EpollServer* new_server, SymmetricEncryptor* new_encryptor);
	HttpApi(std::string new_public_directory, EpollServer* new_server);
	~HttpApi();

	void route(std::string method,
		std::string path,
		std::function<std::string(JsonObject*)> function,
		std::unordered_map<std::string, JsonType> required_parameters = std::unordered_map<std::string, JsonType>(),
		std::chrono::milliseconds rate_limit = std::chrono::milliseconds(0),
		bool requires_human = false
	);

	void form_route(std::string method,
		std::string path,
		std::function<View(JsonObject*)> function,
		std::unordered_map<std::string, JsonType> required_parameters = std::unordered_map<std::string, JsonType>(),
		std::chrono::milliseconds rate_limit = std::chrono::milliseconds(0),
		bool requires_human = false
	);

	void authenticated_route(std::string method,
		std::string path,
		std::function<std::string(JsonObject*, JsonObject*)> function,
		std::unordered_map<std::string, JsonType> required_parameters = std::unordered_map<std::string, JsonType>(),
		std::chrono::milliseconds rate_limit = std::chrono::milliseconds(0),
		bool requires_human = false
	);

	void start(void);
	void set_file_cache_size(int megabytes);
private:
	std::unordered_map<std::string, CachedFile*> file_cache;
	std::unordered_map<std::string, Session*> sessions;
	int file_cache_remaining_bytes = 30 * 1024 * 1024; // 30MB cache.
	std::mutex file_cache_mutex;
	std::string public_directory;
	EpollServer* server;
	SymmetricEncryptor* encryptor;

	std::unordered_map<std::string, Route*> routemap;

	ssize_t respond(int fd, HttpResponse* response);
	ssize_t respond_404(int fd);
	ssize_t respond_cached_file(int fd, HttpResponse* response, CachedFile* cached_file);
	ssize_t respond_parameterized_view(int fd, View* view, HttpResponse* response);
	ssize_t respond_view(int fd, View* view, HttpResponse* response);
	ssize_t respond_http(int fd, View* view, HttpResponse* response);
};
