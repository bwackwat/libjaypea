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
#include "tcp-server.hpp"

static std::unordered_map<std::string, std::string> values;

static int set_value(const char* key_and_value){
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

static std::string get_command_from_request(const char* request){
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

int main(int argc, char** argv){
	int port;

	Util::define_argument("port", &port, {"-p"});
	Util::parse_arguments(argc, argv, "This is a simple key-value server.");

	EpollServer server(static_cast<uint16_t>(port), 10);

	std::string response;
	std::unordered_map<int, int> transaction;

	server.on_connect = [&](int fd){
		transaction[fd] = 0;
	};

	server.on_read = [&](int fd, const char* packet, size_t){
		if(Util::verbose)
			std::cout << "Connection #" << fd << " transaction #" << transaction[fd] << ": " << packet << std::endl;
		
		response = "success";
		if(Util::strict_compare_inequal(packet, get_command, 4) == 0){
			if(values.count(packet + 4) > 0){
				response = values[packet + 4];
			}else{
				response = "failure";
			}
		}else if(Util::strict_compare_inequal(packet, set_command, 4) == 0){
			if(set_value(packet + 4) != 0){
				response = "failure";
			}
		}else if(Util::strict_compare_inequal(packet, exit_command, 4) == 0){
			return true;
		}else{
			if(Util::verbose)
				std::cout << "Unknown command: " << get_command_from_request(packet) << std::endl;
			response = "failure";
		}

		transaction[fd]++;

		if(write(fd, response.c_str(), response.length()) < 0){
			ERROR("write")
			return true;
		}
		return false;
	};

	server.run();

	return 0;
}
