#include <string>
#include <cstring>
#include <iostream>
#include <unordered_map>
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
#include "symmetric-event-server.hpp"

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

		Util::set_non_blocking(shell_pipe[1][0]);

		new_shell->output = shell_pipe[1][0];
		new_shell->input = shell_pipe[0][1];

		return new_shell;
	}
}

static int shell_routine(int socketfd){
	int pid;
	ssize_t res;
	char packet[PACKET_LIMIT];
	std::string res_data;
	// Two pipes for the parent to read and write to socket.
	// Two pipes for child to read and write to parent.
	int shell_pipe[2][2];

	if(pipe(shell_pipe[0]) < 0){
		std::cout << "Uh oh, pipe 0 error.\n";
		return 1;
	}

	if(pipe(shell_pipe[1]) < 0){
		std::cout << "Uh oh, pipe 1 error.\n";
		return 1;
	}

	if((pid = fork()) < 0){
		std::cout << "Uh oh, fork error.\n";
		return 1;
	}else if(pid == 0){
		dup2(shell_pipe[0][0], STDIN_FILENO);
		dup2(shell_pipe[1][1], STDOUT_FILENO);

		close(shell_pipe[0][0]);
		close(shell_pipe[1][1]);
		close(shell_pipe[1][0]);
		close(shell_pipe[0][1]);
	
		if(execl("/bin/bash", "/bin/bash", static_cast<char*>(0)) < 0){
			std::cout << "Uh oh, execl error.\n";
		}
		DEBUG("Done!")
		return 1;
	}else{
		close(shell_pipe[0][0]);
		close(shell_pipe[1][1]);

		// Reading from child shell doesn't block (no output).
		Util::set_non_blocking(shell_pipe[1][0]);
		// Reading from client doesn't block (no input).
		Util::set_non_blocking(socketfd);

		DEBUG("BEGIN EXCHANGE")

		while(1){
			// If child shell outputs, send to socket.
			packet[0] = 0;
			if((res = read(shell_pipe[1][0], packet, PACKET_LIMIT)) < 0){
				DEBUG("Shell read nonblocking.")
				DEBUG_SLEEP(1)
			}else if(res == 0){
				DEBUG("Shell no read. Connection end.")
				DEBUG_SLEEP(1)

				kill(pid, SIGTERM);
				break;
			}else{
				packet[res] = 0;
				if(send(socketfd, std::string(packet))){
					std::cout << "Uh oh, send error.\n";

					kill(pid, SIGTERM);
					break;
				}
				DEBUG("Send to socket:" << packet << "|" << res)
			}

			// If client inputs, send to child shell. 
			packet[0] = 0;
			if((res = read(socketfd, packet, PACKET_LIMIT)) < 0){
				DEBUG("Socket read nonblocking.")
				DEBUG_SLEEP(1)
			}else if(res == 0){
				DEBUG("Socket no read. Connection end.")
				DEBUG_SLEEP(1)

				kill(pid, SIGTERM);
				break;
			}else{
				packet[res] = 0;
				try{
					res_data = decrypt(std::string(packet));
				}catch(const CryptoPP::Exception& e){
					std::cout << e.what() << "\nImplied hacker!\n";

					kill(pid, SIGTERM);
					break;
				}
				if((res = write(shell_pipe[0][1], res_data.c_str(), static_cast<size_t>(res_data.length()))) < 0){
					std::cout << "Uh oh, write error.\n";

					kill(pid, SIGTERM);
					break;
				}
				DEBUG("Send to shell:" << res_data << "|" << res)
			}
		}
	}

	return 0;
}

static std::string IDENTITY = "This is my example identification string.";
static std::string VERIFIED = "Your identity has been verified.";
static std::string START_ROUTINE = "You are welcome to start the requested routine.";
static std::string BAD_ROUTINE = "You have requested an invalid routine!";

enum Routine{
	SHELL,
	SEND_FILE,
	RECV_FILE
};

std::unordered_map<enum Routine, std::string> ROUTINES = {
	{ SHELL, "I would like to use the shell please." },
	{ SEND_FILE, "I would like to send a file please." },
	{ RECV_FILE, "I would like to receive a file please." }
};

enum ComdState{
	GET_IDENTITY,
	GET_ROUTINE,
	GET_PACKET
};

int main(int argc, char** argv){
	enum ComdState state = GET_IDENTITY;
	enum Routine routine = SHELL;
	struct Shell* shell = new struct Shell();
	int port = 3424;
	std::string keyfile = "etc/keyfile";

	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::parse_arguments(argc, argv, "This is a secure server application for com, supporting a remote shell, receive file, and send file routines.");

	SymmetricEventServer server(keyfile, static_cast<uint16_t>(port), 1);

	// This "state" is not per-client, because comd is designed for a single connection.
	server.on_connect = [&](int fd){
		state = GET_IDENTITY;
	};

	// Easy DDOS protection. (This is not a multi-user tool.)
	server.on_disconnect = [](int fd){
		server.on_event_loop = nullptr;
		sleep(10);
	};
	
	server.run([&](int fd, char* data, size_t data_length){
		switch(state){
		case GET_IDENTITY:
			if(std::strcmp(data, IDENTITY.c_str()) != 0){
				PRINT("Client provided bad identity.")
				return true;
			}
			if(server.send(fd, VERIFIED.c_str(), VERIFIED.length())){
				return true;
			}
			state = GET_ROUTINE;
			break;
		case GET_ROUTINE:
			if(std::strcmp(data, ROUTINES[SHELL].c_str()) == 0){
				if(server.send(fd, START_ROUTINE.c_str(), START_ROUTINE.length())){
					return true;
				}
				if((shell_pipes = shell_routine()) == 0){
					return true;
				}
				delete shell;
				if((shell = shell_routine()) == 0){
					return true;
				}
				server.on_event_loop = [&](){
					
				};
				routine = SHELL;
			}else if(std::strcmp(data, ROUTINES[SEND_FILE].c_str()) == 0){
				server.send(fd, BAD_ROUTINE.c_str(), BAD_ROUTINE.length());
				return true;
			}else if(std::strcmp(data, ROUTINES[RECV_FILE].c_str()) == 0){
				server.send(fd, BAD_ROUTINE.c_str(), BAD_ROUTINE.length());
				return true;
			}else{
				server.send(fd, BAD_ROUTINE.c_str(), BAD_ROUTINE.length());
				return true;
			}
			state = GET_PACKET;
			break;
		case GET_PACKET:
			switch(routine){
			case SHELL:
		}
		return false;
	}
	// Should not end.
}
