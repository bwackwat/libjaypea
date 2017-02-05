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
#include "comd-util.hpp"
#include "symmetric-tcp-server.hpp"

struct Shell{
	int pid;
	int input;
	int output;
};

static struct Shell* shell_routine(){
	struct Shell* new_shell = new struct Shell();
	int shell_pipe[2][2];

	if(pipe(shell_pipe[0]) < 0){
		ERROR("pipe 0")
		delete new_shell;
		return 0;
	}
	if(pipe(shell_pipe[1]) < 0){
		ERROR("pipe 0")
		delete new_shell;
		return 0;
	}
	if((new_shell->pid = fork()) < 0){
		ERROR("fork")
		delete new_shell;
		return 0;
	}else if(new_shell->pid == 0){
		dup2(shell_pipe[0][0], STDIN_FILENO);
		dup2(shell_pipe[1][1], STDOUT_FILENO);

		close(shell_pipe[0][0]);
		close(shell_pipe[1][1]);
		close(shell_pipe[1][0]);
		close(shell_pipe[0][1]);

		if(execl("/bin/bash", "/bin/bash", static_cast<char*>(0)) < 0){
			ERROR("execl")
		}
		PRINT("execl done!")
		delete new_shell;
		return 0;
	}else{
		close(shell_pipe[0][0]);
		close(shell_pipe[1][1]);

		//Util::set_non_blocking(shell_pipe[1][0]);

		new_shell->output = shell_pipe[1][0];
		new_shell->input = shell_pipe[0][1];

		return new_shell;
	}
}

int main(int argc, char** argv){
	enum ComdState state = VERIFY_IDENTITY;
	enum ComdRoutine routine = SHELL;
	struct Shell* shell = shell_routine();
	if(shell == 0){
		ERROR("shell_routine")
		return -1;
	}
	int shell_client_fd;
	char packet[PACKET_LIMIT];
	ssize_t len;
	int port;
	std::string keyfile;

	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::parse_arguments(argc, argv, "This is a secure server application for com, supporting a remote shell, receive file, and send file routines.");

	SymmetricEpollServer server(keyfile, static_cast<uint16_t>(port), 1);

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
				if(shell == 0){
					shell = shell_routine();
					if(shell == 0){
						ERROR("shell_routine recall")
					}
				}
				if(shell != 0){
					if((len = write(shell->input, data, static_cast<size_t>(data_length))) < 0){
						ERROR("shell input write")
						return -1;
					}
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
		sleep(10);
	};

	// Returns and is single threaded.
	server.run(true, 1);

	auto close_shell = [&](){
		kill(shell->pid, SIGTERM);
		close(shell->input);
		close(shell->output);
		delete shell;
		shell = 0;
	};

	while(true){
		if(shell == 0){
			continue;
		}
		if((len = read(shell->output, packet, PACKET_LIMIT)) <= 0){
			ERROR("shell read")
			close_shell();
		}else if(len == 0){
			PRINT("shell output read zero")
			close_shell();
		}else{
			packet[len] = 0;
			if(shell != 0){
				if(server.send(shell_client_fd, packet, static_cast<size_t>(len))){
					close_shell();
				}
			}
		}
	}
}
