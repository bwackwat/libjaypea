#include <string>
#include <cstring>
#include <iostream>
#include <map>
#include <thread>
#include <cstdlib>

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.hpp"
#include "shell.hpp"
#include "comd-util.hpp"
#include "symmetric-epoll-server.hpp"

int main(int argc, char** argv){
	enum ComdState state = VERIFY_IDENTITY;
	enum ComdRoutine routine = SHELL;
	int shell_client_fd;
	char packet[PACKET_LIMIT];
	ssize_t len;
	int port;
	std::string keyfile;

	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::parse_arguments(argc, argv, "This is a secure server application for com, supporting a remote shell, receive file, and send file routines.");

	SymmetricEpollServer server(keyfile, static_cast<uint16_t>(port), 1);

	Shell shell;

	// This "state" is not per-client, because comd is designed for a single connection.
	server.on_connect = [&](int){
		state = VERIFY_IDENTITY;
	};

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		switch(state){
		case VERIFY_IDENTITY:
			PRINT(data)
			if(std::strcmp(data, IDENTITY.c_str()) != 0){
				PRINT("Client provided bad identity.")
				return -1;
			}
			if(server.send(fd, VERIFIED.c_str(), VERIFIED.length())){
				return -1;
			}
			state = SELECT_ROUTINE;
			break;
		case SELECT_ROUTINE:
			PRINT(data)
			if(std::strcmp(data, ROUTINES[SHELL].c_str()) == 0){
				if(server.send(fd, START_ROUTINE.c_str(), START_ROUTINE.length())){
					return -1;
				}
				shell_client_fd = fd;
				routine = SHELL;
			}else if(std::strcmp(data, ROUTINES[SEND_FILE].c_str()) == 0){
				server.send(fd, BAD_ROUTINE.c_str(), BAD_ROUTINE.length());
				return -1;
			}else if(std::strcmp(data, ROUTINES[RECV_FILE].c_str()) == 0){
				server.send(fd, BAD_ROUTINE.c_str(), BAD_ROUTINE.length());
				return -1;
			}else{
				server.send(fd, BAD_ROUTINE.c_str(), BAD_ROUTINE.length());
				return -1;
			}
			state = EXCHANGE_PACKETS;
			break;
		case EXCHANGE_PACKETS:
			switch(routine){
			case SHELL:
				if(shell.sopen()){
					PRINT("Couldn't open shell!")
					return -1;
				}
				if((len = write(shell.input, data, static_cast<size_t>(data_length))) < 0){
					ERROR("shell input write")
					return -1;
				}
				break;
			case SEND_FILE:
			case RECV_FILE:
				return -1;
			}
		}
		return data_length;
	};

	// Easy DDOS protection. (This is not a multi-user tool.)
	server.on_disconnect = [&](int){
		shell.sclose();
		sleep(10);
	};

	// Returns and is single threaded.
	server.run(true, 1);

	while(true){
		if(!shell.opened){
			continue;
		}
		if((len = read(shell.output, packet, PACKET_LIMIT)) <= 0){
			ERROR("shell read")
			shell.sclose();
		}else if(len == 0){
			PRINT("shell output read zero")
			shell.sclose();
		}else{
			packet[len] = 0;
			if(server.send(shell_client_fd, packet, static_cast<size_t>(len))){
				shell.sclose();
			}
		}
	}
}
