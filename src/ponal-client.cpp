#include <string>
#include <iostream>
#include <cstring>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ponal.hpp"

int get_line(std::istream& is, std::string& result){
	std::streambuf* sb = is.rdbuf();
	result.clear();
	
	for(int i = 0;; i++){
		int c = sb->sbumpc();
		switch (c) {
		case '\n':
			return i;
		case '\r':
			if(sb->sgetc() == '\n')
				sb->sbumpc();
			return i;
		case EOF:
			if(result.empty()){
				is.setstate(std::ios::eofbit);
			}
			return i;
		default:
			result += static_cast<char>(c);
		}
	}
}

int main(int argc, char** argv){
	std::string hostname = "localhost";
	uint16_t port = 4767;
	int terminal = isatty(fileno(stdin));

	for(int i = 0; i < argc; i++){
		if(std::strcmp(argv[i], "--hostname") == 0 && argc > i){
			hostname = argv[i + 1];
		}
		if(std::strcmp(argv[i], "--port") == 0 && argc > i){
			port = static_cast<uint16_t>(std::stoi(argv[i + 1]));
		}
	}

	Ponal ponal(port, hostname);

	std::string request;
	std::string response;
	while(1){
		if(terminal)
			std::cout << "> ";

		if(get_line(std::cin, request) == 0){
			continue;
		}

		response = ponal.Communicate(request.c_str(), request.length());

		std::cout << response;
		if(terminal)
			std::cout << std::endl;
		
		if(!terminal)
			break;
	}

	return 0;
}
