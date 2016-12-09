#include <string>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <thread>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.hpp"
#include "simple-tcp-server.hpp"

static std::unordered_map<std::string, std::string> values;
static int loud = 1;

static int set_value(char* key_and_value){
	int get_key = 1;
	std::string key = "";
	std::string value = "";
	for(const char* it = key_and_value; *it; ++it){
		if(*it == ' ' && get_key){
			get_key = 0;
			continue;
		}
		if(get_key){
			key += *it;
		}else{
			value += *it;
		}
	}
	if(key.length() == 0 || value.length() == 0){
		return 1;
	}
	values[key] = value;
	return 0;
}

static std::string get_command_from_request(char* request){
	std::string command = "";
	for(const char* it = request; *it; ++it){
		if(*it == ' '){
			return command;
		}else{
			command += *it;
		}
	}
	return command;
}

static const char* get_command = "get ";
static const char* set_command = "set ";
static const char* exit_command = "exit";

static void handle_connection(int fd, int hit){
	int transaction;
	ssize_t len;
	char request[PACKET_LIMIT];
	std::string response;

	for(transaction = 1;; transaction++){
		if((len = read(fd, request, PACKET_LIMIT)) <= 0){
			break;
		}
		request[len] = 0;

		if(loud)
			std::cout << "Connection #" << hit << " transaction #" << transaction << ": " << request << std::endl;
		
		response = "success";
		if(Util::strict_compare_inequal(request, get_command, 4) == 0){
			if(values.count(request + 4) > 0){
				response = values[request + 4];
			}else{
				response = "failure";
			}
		}else if(Util::strict_compare_inequal(request, set_command, 4) == 0){
			if(set_value(request + 4) != 0){
				response = "failure";
			}
		}else if(Util::strict_compare_inequal(request, exit_command, 4) == 0){
			break;
		}else{
			if(loud)
				std::cout << "Unknown command: " << get_command_from_request(request) << std::endl;
			response = "failure";
		}

		if(write(fd, response.c_str(), response.length()) < 0){
			break;
		}
	}
	if(loud)
		std::cout << "Connection #" << hit << " transaction #" << transaction << " is done!\n";
	close(fd);
}

int main(int argc, char** argv){
	int socketfd;
	uint16_t port = 4767;

	for(int i = 0; i < argc; i++){
                if(std::strcmp(argv[i], "--port") == 0 && argc > i){
                        port = static_cast<uint16_t>(std::stoi(argv[i + 1]));
                }
		if(std::strcmp(argv[i], "--quiet") == 0){
                        loud = 0;
                }
        }

	SimpleTcpServer server(port);
	
	if(loud)
		std::cout << "Ponal is running.\n";

	while(1){
		socketfd = server.accept_connection();

		auto thread = std::thread(handle_connection, socketfd, server.hit);
		thread.detach();
	}
	// Should not end.
}
