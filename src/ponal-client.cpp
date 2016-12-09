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

#include "simple-tcp-client.hpp"
#include "util.hpp"

static int get_line(std::istream& is, std::string& result){
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
	int terminal = isatty(fileno(stdin));
	std::string hostname = "localhost";
	int port = 4767;

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("port", &port, {"-p"});
	Util::parse_arguments(argc, argv, "This is a simple client to ponald for command-line communication.");

	SimpleTcpClient client(hostname, static_cast<uint16_t>(port));

	std::string request;
	char response[PACKET_LIMIT];
	while(1){
		if(terminal)
			std::cout << "> ";

		if(get_line(std::cin, request) == 0){
			continue;
		}

		client.communicate(request.c_str(), request.length(), response);

		std::cout << response;
		if(terminal)
			std::cout << std::endl;
		
		if(!terminal)
			break;
	}

	return 0;
}
