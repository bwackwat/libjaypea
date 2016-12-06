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

#define PACKET_LIMIT 2048

std::unordered_map<std::string, std::string> values;
int loud = 1;

int strict_compare(const char* first, const char* second, int count){
	for(int i = 0; i < count; ++i){
		if(first[i] != second[i]){
			return 1;
		}
	}
	return 0;
}

int set_value(char* key_and_value){
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

std::string get_command_from_request(char* request){
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

const char* get_command = "get ";
const char* set_command = "set ";
const char* exit_command = "exit";

void handle_connection(int fd, int hit){
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
		if(strict_compare(request, get_command, 4) == 0){
			if(values.count(request + 4) > 0){
				response = values[request + 4];
			}else{
				response = "failure";
			}
		}else if(strict_compare(request, set_command, 4) == 0){
			if(set_value(request + 4) != 0){
				response = "failure";
			}
		}else if(strict_compare(request, exit_command, 4) == 0){
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
	int res, listenfd, socketfd, hit;
	socklen_t length;
	struct sockaddr_in cli_addr;
	struct sockaddr_in serv_addr;
	uint16_t port = 4767;

	for(int i = 0; i < argc; i++){
                if(std::strcmp(argv[i], "--port") == 0 && argc > i){
                        port = static_cast<uint16_t>(std::stoi(argv[i + 1]));
                }
		if(std::strcmp(argv[i], "--quiet") == 0){
                        loud = 0;
                }
        }

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		std::cout << "System call to socket failed!\n" << listenfd;
		return 1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if((res = bind(listenfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr))) < 0){
		std::cout << "System call to bind failed!\n" << res << errno;
		return 1;
	}

	if(listen(listenfd, 10) < 0){
		std::cout << "System call to listen failed!\n";
		return 1;
	}
	
	if(loud)
		std::cout << "Ponal is running.\n";

	for(hit = 1;; hit++){
		length = sizeof(cli_addr);

		if((socketfd = accept(listenfd, reinterpret_cast<struct sockaddr*>(&cli_addr), &length)) < 0){
			std::cout << "System call to accept failed!\n";
			return 1;
		}

		auto thread = std::thread(handle_connection, socketfd, hit);
		thread.detach();
	}
	// Should not end.
}
