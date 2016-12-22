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

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "util.hpp"
#include "json.hpp"
#include "tcp-event-server.hpp"
#include "private-event-server.hpp"

#define BUFFER_LIMIT 8192
#define HTTP_404 "<h1>404 Not Found</h1>"

static JsonObject* request_object;
static std::string hostname;

static bool parse_request(char* request){
	std::string new_key;
	std::string new_value;
	//0 = No state
	//1 = Getting route
	//2 = Getting route parameter key
	//3 = Getting route parameter value
	//4 = Got route
	//4+ = Finding newline or carriage return
	//8 = Found \r\n\r\n, thus the HTTP data
	int state = 0;
	char* it;

	delete request_object;
	request_object = new JsonObject();
	request_object->type = OBJECT;
	request_object->objectValues["route"] = new JsonObject();
	request_object->objectValues["route"]->type = STRING;

	// Get the route.
	for(it = request; *it; ++it){
		switch(*it){
		case ' ':
			if(state == 0){
				new_value = "";
				state = 1;
				continue;
			}else if(state == 1){
				request_object->objectValues["route"]->type = STRING;
				request_object->objectValues["route"]->stringValue = new_value;
				state = 4;
			}else if(state == 2){
				state = 4;
			}else if(state == 3){
				if(!request_object->objectValues.count(new_key)){
					request_object->objectValues[new_key] = new JsonObject();
				}
				request_object->objectValues[new_key]->type = STRING;
				request_object->objectValues[new_key]->stringValue = new_value;
				state = 4;
			}
			break;
		case '?':
			if(state == 1){
				request_object->objectValues["route"]->type = STRING;
				request_object->objectValues["route"]->stringValue = new_value;
				new_key = "";
				state = 2;
				continue;
			}
			break;
		case '=':
			if(state == 2){
				new_value = "";
				state = 3;
				continue;
			}
			break;
		case '&':
			if(state == 3){
				if(!request_object->objectValues.count(new_key)){
					request_object->objectValues[new_key] = new JsonObject();
				}
				request_object->objectValues[new_key]->type = STRING;
				request_object->objectValues[new_key]->stringValue = new_value;
				new_key = "";
				state = 2;
				continue;
			}
			break;
		}

		if(state == 1){
			new_value += *it;
		}else if(state == 2){
			new_key += *it;
		}else if(state == 3){
			new_value += *it;
		}else if(state >= 4){
			if(*it == '\n' ||
			*it == '\r'){
				state++;
			}else{
				state = 4;
			}
		}

		// Parse until 4 \r or \n's have been reached (data is here).
		if(state >= 8){
			break;
		}
	}

	if(request_object->objectValues["route"]->stringValue.length() >= 4 &&
	request_object->objectValues["route"]->stringValue.substr(0, 4) == "/api"){
		// Data is JSON or trash, parse it.
		request_object->parse(it);
		return true;
	}
	return false;
}

static std::unordered_map<std::string, std::string(*)(JsonObject* json)> routemap;
static std::unordered_map<std::string, std::unordered_map<std::string, JsonType>> required_fields;

static void route(std::string path, std::string(*func)(JsonObject* json), std::unordered_map<std::string, JsonType> requires = std::unordered_map<std::string, JsonType>()){
	if(path[path.length() - 1] != '/'){
		path += '/';
	}
	routemap[path] = func;
	required_fields[path] = requires;
}

static std::string root(JsonObject*){
	return "{\"result\":\"Welcome to the API!\"}";
}

static std::string routes_string;
static std::string routes(JsonObject*){
	return routes_string;
}

static pid_t http_redirect(){
	pid_t pid;
	
	// Trivial 301 Redirect.	
	std::string response_body = "<html>\n"
		"<head>\n"
		"<title>Moved</title>\n"
		"</head>\n"
		"<body>\n"
		"<h1>Moved</h1>\n"
		"<p>This page has moved to <a href=\"https://" +
		hostname +
		"/\">https://" +
		hostname +
		"/</a>.</p>\n"
		"</body>\n"
		"</html>";
	std::string response = "HTTP/1.1 301 Moved Permanently\n" 
		"Location: https://" + hostname + "/\n"
		"Content-Type: text/html\n"
		"Content-Length: " + std::to_string(response_body.length()) + "\n"
		"\n\n" +
		response_body;
	
	if((pid = fork()) < 0){
		std::cout << "Uh oh, fork error!" << std::endl;
		return -1;
	}else if(pid > 0){
		try{
			EventServer server("HttpRedirecter", 80, 10);
			server.run([&](int fd, const char*, size_t){
				write(fd, response.c_str(), response.length());
				return true;
			});
		}catch(std::exception& e){
			std::cout << e.what() << std::endl;
			if(kill(pid, SIGTERM) < 0){
				std::cout << "Kill redirect fork error." << std::endl;
			}
			return 1;
		}
	}

	return pid;
}		

static pid_t http_redirect_pid;

int main(int argc, char **argv){
	std::string public_directory;
	std::string ssl_certificate = "etc/ssl/ssl.crt";
	std::string ssl_private_key = "etc/ssl/ssl.key";

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("hostname", hostname, {"-hn"});
	Util::parse_arguments(argc, argv, "This is a modern web server monad which starts an HTTP redirection server, an HTTPS server for files, and a JSON API. Configured via etc/configuration.json.");

	if((http_redirect_pid = http_redirect()) < 0){
		std::cout << "Uh oh, http_redirect() error." << std::endl;
		return 1;
	}

	int clientfd, res;
	ssize_t len;
	struct stat route_stat;
	SSL_CTX* ctx = 0;
	SSL* ssl = 0;
	char buffer[BUFFER_LIMIT];
	std::string response_header = "HTTP/1.1 200 OK\n"
		"Accept-Ranges: bytes\n"
		"Content-Type: text\n";
	std::string response_length;
	std::string response_body;

	route("/", root);
	route("/routes", routes);

	JsonObject* routes_object = new JsonObject();
	routes_object->type = OBJECT;
	routes_object->objectValues["result"] = new JsonObject();
	routes_object->objectValues["result"]->type = STRING;
	routes_object->objectValues["result"]->stringValue = "This is a list of the routes and their required JSON parameters";
	JsonObject* routes_sub_object = new JsonObject();
	routes_sub_object->type = OBJECT;
	for(auto iter = routemap.begin(); iter != routemap.end(); ++iter){
		routes_sub_object->objectValues[iter->first] = new JsonObject();
		routes_sub_object->objectValues[iter->first]->type = ARRAY;
		for(auto &field : required_fields[iter->first]){
			JsonObject* array_item = new JsonObject();
			array_item->type = STRING;
			array_item->stringValue = field.first;
			routes_sub_object->objectValues[iter->first]->arrayValues.push_back(array_item);
		}
	}
	routes_object->objectValues["routes"] = routes_sub_object;
	routes_string = routes_object->stringify(true);
	delete routes_object;

	try{
		PrivateEventServer server(ssl_certificate, ssl_private_key, 443, 10);
		server.run([&](int fd, const char* data, size_t data_length)
	{
		if(server.send(fd, response_header.c_str(), static_cast<int>(response_header.length()))){
			return true;
		}
		PRINT("DELI:" << response_header)
		std::string response_body = "";

		if(parse_request(data)){
			// JSON API request.
			std::string api_route = request_object->objectValues["route"]->stringValue.substr(4);
			if(api_route[api_route.length() - 1] != '/'){
				api_route += '/';
			}
			std::cout << "APIR:" << api_route;

			if(routemap.count(api_route)){
				bool good_call = true;
				for(auto iter = required_fields[api_route].begin(); iter != required_fields[api_route].end(); ++iter){
					if(!request_object->objectValues.count(iter->first) ||
					request_object->objectValues[iter->first]->type != iter->second){
						// Missing parameter
						response_body = "{\"error\":\"'" + iter->first + "' requries a " + JsonObject::typeString[iter->second] + ".\"}";
						good_call = false;
						break;
					}
				}
				if(good_call){
					response_body = routemap[api_route](request_object);
				}
			}else{
				response_body = "{\"error\":\"Invalid API route.\"}";
			}
		}else{
			// "Regular" HTTP request.
			std::string clean_route = public_directory;
			for(size_t i = 0; i < request_object->objectValues["route"]->stringValue.length(); ++i){
				if(i < request_object->objectValues["route"]->stringValue.length() - 1 &&
				request_object->objectValues["route"]->stringValue[i] == '.' &&
				request_object->objectValues["route"]->stringValue[i + 1] == '.'){
					continue;
				}
				clean_route += request_object->objectValues["route"]->stringValue[i];
			}

			if(lstat(clean_route.c_str(), &route_stat) < 0){
				ERROR("lstat")
				response_body = HTTP_404;
			}else if(S_ISDIR(route_stat.st_mode)){
				clean_route += "/index.html";
				if(lstat(clean_route.c_str(), &route_stat) < 0){
					ERROR("lstat index.html")
					response_body = HTTP_404;
				}
			}
			if(response_body.empty() && S_ISREG(route_stat.st_mode)){
				PRINT("SEND FILE:" << clean_route)
				response_length = "Content-Length: " + std::to_string(route_stat.st_size) + "\n\n";
				if(server.send(fd, response_length.c_str(), static_cast<int>(response_length.length()))){
					return true;
				}
				PRINT("DELI:" << response_length)

				int fd;
				if((fd = open(clean_route.c_str(), O_RDONLY)) < 0){
					ERROR("open file")
					return false;
				}
				while(1){
					if((len = read(fd, buffer, BUFFER_LIMIT)) < 0){
						ERROR("read file")
						return false;
					}
					buffer[len] = 0;
					if(server.send(fd, buffer, static_cast<int>(len))){
						return true;
					}
					if(len < BUFFER_LIMIT){
						break;
					}
				}
				if(close(fd) < 0){
					ERROR("close file")
					return false;
				}
			}else{
				PRINT("Something other than a regular file was requested...")
				response_body = HTTP_404;
			}
		}
		if(!response_body.empty()){
			response_length = "Content-Length: " + std::to_string(response_body.length()) + "\n\n";
			if(server.send(fd, response_length.c_str(), static_cast<int>(response_length.length()))){
				return true;
			}
			PRINT("DELI:" << response_length)
			if(server.send(fd, response_body.c_str(), static_cast<int>(response_body.length()))){
				return true;
			}
			PRINT("DELI:" << response_body)
		}

		PRINT("JSON:" << request_object->stringify(true))
	}
	}catch(std::exception& e){
		PRINT(e.what())
		if(kill(http_redirect_pid, SIGTERM) < 0){
			ERROR("kill redirecter")
		}
	}
}