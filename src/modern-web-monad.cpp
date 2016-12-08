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

#include "json.hpp"
#include "simple-tcp-server.hpp"

#define BUFFER_LIMIT 8192
#define HTTP_404 "<h1>404 Not Found</h1>"

JsonObject* request_object;
JsonObject config_object;

bool parse_request(char* request){
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

std::unordered_map<std::string, std::string(*)(JsonObject* json)> routemap;
std::unordered_map<std::string, std::unordered_map<std::string, JsonType>> required_fields;

void route(std::string path, std::string(*func)(JsonObject* json), std::unordered_map<std::string, JsonType> requires = std::unordered_map<std::string, JsonType>()){
	if(path[path.length() - 1] != '/'){
		path += '/';
	}
	routemap[path] = func;
	required_fields[path] = requires;
}

std::string root(JsonObject* json){
	return "{\"result\":\"Welcome to the API!\"}";
}

std::string routes_string;
std::string routes(JsonObject* json){
	return routes_string;
}

pid_t http_redirect(){
	int clientfd;
	ssize_t len;
	pid_t pid;
	char buffer[BUFFER_LIMIT];
	
	// Trivial 301 Redirect.	
	std::string response_body = "<html>\n"
		"<head>\n"
		"<title>Moved</title>\n"
		"</head>\n"
		"<body>\n"
		"<h1>Moved</h1>\n"
		"<p>This page has moved to <a href=\"https://" +
		config_object.objectValues["server_hostname"]->stringValue +
		"/\">https://" +
		config_object.objectValues["server_hostname"]->stringValue +
		"/</a>.</p>\n"
		"</body>\n"
		"</html>";
	std::string response = "HTTP/1.1 301 Moved Permanently\n" 
		"Location: https://" + config_object.objectValues["server_hostname"]->stringValue + "/\n"
		"Content-Type: text/html\n"
		"Content-Length: " + std::to_string(response_body.length()) + "\n"
		"\n\n" +
		response_body;
	
	if((pid = fork()) < 0){
		std::cout << "Uh oh, fork error!" << std::endl;
		return -1;
	}else if(pid > 0){
		try{
			SimpleTcpServer server(80);
		        while(1){
				clientfd = server.accept_connection();
				if((len = read(clientfd, buffer, BUFFER_LIMIT)) < 0){
					std::cout << "Uh oh, redirect read error!" << std::endl;
					continue;
				}
				buffer[len] = 0;
				std::cout << "REDI:" << buffer << '|' << len << std::endl;
				if((len = write(clientfd, response.c_str(), response.length())) < 0){
					std::cout << "Uh oh, redirect write error!" << std::endl;
					continue;
				}
				if(close(clientfd) < 0){
					std::cout << "Uh oh, redirect close error!" << std::endl;
					continue;
				}
			}
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

pid_t http_redirect_pid;

int ssl_send(SSL* ssl, const char* buffer, int length){
	int len = SSL_write(ssl, buffer, length);
	switch(SSL_get_error(ssl, len)){
	case SSL_ERROR_NONE:
		break;
	default:
		std::cout << "SSL write issue..." << std::endl;
		SSL_free(ssl);
		return -1;
	}
	if(len != length){
		std::cout << "SSL wrote odd number of bytes:" << len << '|' << length << std::endl;
		return -1;
	}
	return len;
}

int main(int argc, char **argv){
	std::ifstream config_file("etc/configuration.json");
        std::string config_data((std::istreambuf_iterator<char>(config_file)),
                (std::istreambuf_iterator<char>()));
        config_object.parse(config_data.c_str());

        std::cout << "Loaded: configuration.json" << std::endl;
        std::cout << config_object.stringify(true) << std::endl;

	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "-h") == 0 ||
		std::strcmp(argv[i], "--help") == 0){
			std::cout << "This is configured via etc/configuration.json." << std::endl;
		}
	}

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

	SSL_load_error_strings();
	SSL_library_init();

        if((ctx = SSL_CTX_new(SSLv23_server_method())) == 0){
                std::cout << "Uh oh, unable to create SSL context." << std::endl;
                ERR_print_errors_fp(stdout);
                return -1;
        }

        if(SSL_CTX_set_ecdh_auto(ctx, 1) == 0){
		std::cout << "Uh oh, SSL_CTX_set_ecdh_auto(ctx, 1) error." << std::endl;
		return -1;
	}

        if(SSL_CTX_use_certificate_file(ctx, "etc/ssl/ssl.crt", SSL_FILETYPE_PEM) != 1){
                std::cout << "Certificate file 'ssl.crt' error." << std::endl;
                return -1;
        }

        if(SSL_CTX_use_PrivateKey_file(ctx, "etc/ssl/ssl.key", SSL_FILETYPE_PEM) != 1){
                std::cout << "Private key file 'ssl.key' error." << std::endl;
                return -1;
        }

	/*
	 *
	 * NOTE: I could not refactor the SSL init code above into another function...
	 * There was an undetermined issue with this, causing SSL_new to fail.
	 * I believe it has to do with SSL_library_init() creating scoped variables.
	 *
	 */

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
		SimpleTcpServer server(443);
	while(1){
		clientfd = server.accept_connection();

		if((ssl = SSL_new(ctx)) == 0){
			std::cout << "SSL_new error!" << std::endl;
			ERR_print_errors_fp(stdout);
			SSL_free(ssl);
			close(clientfd);
			break;
		}

		SSL_set_fd(ssl, clientfd);

		if(SSL_accept(ssl) <= 0){
			std::cout << "SSL_accept error!" << std::endl;
			ERR_print_errors_fp(stdout);
			SSL_free(ssl);
			close(clientfd);
			continue;
		}

		// If this can't fit the entire request then forget it.
		// I don't want to be accepting requests larger than this.
		res = SSL_read(ssl, buffer, BUFFER_LIMIT);
		switch(SSL_get_error(ssl, res)){
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_ZERO_RETURN:
			std::cout << "Nothing read? End connection?" << std::endl;
			break;
		default:
			std::cout << "SSL read issue?" << std::endl;
			SSL_free(ssl);
			close(clientfd);
			continue;
		}
		buffer[res] = 0;
		std::cout << "RECV:" << buffer << '|' << res << std::endl;
		response_body = "";

		if((res = ssl_send(ssl, response_header.c_str(), static_cast<int>(response_header.length()))) < 0){
			std::cout << "SSL send header error." << std::endl;
			close(clientfd);
			continue;
		}
		std::cout << "DELI:" << response_header << '|' << res << std::endl;

		if(parse_request(buffer)){
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
			std::string clean_route = config_object.objectValues["public_directory"]->stringValue;
			for(size_t i = 0; i < request_object->objectValues["route"]->stringValue.length(); ++i){
				if(i < request_object->objectValues["route"]->stringValue.length() - 1 &&
				request_object->objectValues["route"]->stringValue[i] == '.' &&
				request_object->objectValues["route"]->stringValue[i + 1] == '.'){
					continue;
				}
				clean_route += request_object->objectValues["route"]->stringValue[i];
			}

			if(lstat(clean_route.c_str(), &route_stat) < 0){
				std::cout << "Uh oh, lstat error!" << std::endl;
				response_body = HTTP_404;
			}else if(S_ISDIR(route_stat.st_mode)){
				clean_route += "/index.html";
				if(lstat(clean_route.c_str(), &route_stat) < 0){
					std::cout << "Uh oh, lstat index.html error!" << std::endl;
					response_body = HTTP_404;
				}
			}
			if(response_body.empty() && S_ISREG(route_stat.st_mode)){
				std::cout << "SEND FILE:" << clean_route << std::endl;

				response_length = "Content-Length: " + std::to_string(route_stat.st_size) + "\n\n";
				if((res = ssl_send(ssl, response_length.c_str(), static_cast<int>(response_length.length()))) < 0){
					std::cout << "SSL send length error." << std::endl;
					close(clientfd);
					continue;
				}
				std::cout << "DELI:" << response_length << '|' << res << std::endl;

				int fd;
				if((fd = open(clean_route.c_str(), O_RDONLY)) < 0){
					std::cout << "Uh oh, open file error!" << std::endl;
					return 1;
				}
				while(1){
					if((len = read(fd, buffer, BUFFER_LIMIT)) < 0){
						std::cout << "Uh oh, read file error!" << std::endl;
						return 1;
					}
					buffer[len] = 0;
					if((res = ssl_send(ssl, buffer, static_cast<int>(len))) < 0){
						std::cout << "SSL send buffer of data." << std::endl;
						close(clientfd);
						break;
					}
					if(len < BUFFER_LIMIT){
						break;
					}
				}
				if(close(fd) < 0){
					std::cout << "Uh oh, close file error!" << std::endl;
					return 1;
				}
			}else{
				std::cout << "Something other than a regular file was requested..." << std::endl;
				response_body = HTTP_404;
			}
		}
		if(!response_body.empty()){
			response_length = "Content-Length: " + std::to_string(response_body.length()) + "\n\n";
			if((res = ssl_send(ssl, response_length.c_str(), static_cast<int>(response_length.length()))) < 0){
				std::cout << "SSL send length error." << std::endl;
				close(clientfd);
				continue;
			}
			std::cout << "DELI:" << response_length << '|' << res << std::endl;
			if((res = SSL_write(ssl, response_body.c_str(), static_cast<int>(response_body.length()))) < 0){
				std::cout << "SSL send body error." << std::endl;
				close(clientfd);
				continue;
			}
			std::cout << "DELI:" << response_body << '|' << res << std::endl;
		}

		std::cout << "JSON:" << request_object->stringify(true) << std::endl;
		
		SSL_free(ssl);
		close(clientfd);
	}
	}catch(std::exception& e){
		std::cout << e.what() << std::endl;
		if(kill(http_redirect_pid, SIGTERM) < 0){
			std::cout << "Kill redirect fork error." << std::endl;
		}
	}
	SSL_CTX_free(ctx);
	EVP_cleanup();
}
