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
#include "simple-tcp-server.hpp"

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

int main(int argc, char** argv){
	int socketfd;
	ssize_t res;
	char packet[PACKET_LIMIT];

	std::string req_data;
	std::string response;
	std::string res_data;

	int port = 3424;
	std::string keyfile = "etc/keyfile";

	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::parse_arguments(argc, argv, "This is a secure server application for com, supporting a remote shell, receive file, and send file routines.");

	if(init_crypto(keyfile)){
		return 1;
	}

	SimpleTcpServer server(static_cast<uint16_t>(port), 0);

	while(1){
		// Easy DDOS protection. (This is not a multi-user tool.)
		if(server.hit != 1){
			sleep(10);
		}
		std::cout << "Accepting a new connection." << std::endl;

		socketfd = server.accept_connection();

		// Handshake step 1:
		// Check that identity!

		DEBUG("reading..")
		packet[0] = 0;
		if((res = read(socketfd, packet, PACKET_LIMIT)) < 0){
			DEBUG("read error.")
			DEBUG_SLEEP(1);
			std::cout << "Uh oh, read identity error!" << std::endl;
			close(socketfd);
			continue;
		}else if(res == 0){
			DEBUG("read 0?")
		}
		packet[res] = 0;
		DEBUG("read!" << packet << "|" << res)
		try{
			req_data = decrypt(std::string(packet));
		}catch(const CryptoPP::Exception& e){
			std::cout << e.what() << "\nImplied hacker! Done" << std::endl;
			close(socketfd);
			continue;
		}
		std::cout << req_data << std::endl;

		if(std::strcmp(req_data.c_str(), IDENTITY.c_str()) != 0){
			// Unknown connector. Close connection.
			std::cout << "Connection provided bad verification! Done." << std::endl;
			close(socketfd);
			continue;
		}

		std::cout << "Authorized!" << std::endl;

		if(send(socketfd, VERIFIED)){
			std::cout << "Uh oh, send verification error." << std::endl;
			close(socketfd);
			continue;
		}

		// Handshake step 2:
		// Get method!

		packet[0] = 0;
		if((res = read(socketfd, packet, PACKET_LIMIT)) < 0){
			std::cout << "Uh oh, read method error!" << std::endl;
			close(socketfd);
			continue;
		}
		packet[res] = 0;
		req_data = decrypt(packet);
		std::cout << req_data << std::endl;
		
		if(std::strcmp(req_data.c_str(), ROUTINES[SHELL].c_str()) == 0){
			if(send(socketfd, START_ROUTINE)){
				std::cout << "Uh oh, send start routine error." << std::endl;;
				close(socketfd);
				continue;
			}
			if(shell_routine(socketfd)){
				std::cout << "Uh oh, shell routine error!" << std::endl;
				close(socketfd);
				continue;
			}
		}else if(std::strcmp(req_data.c_str(), ROUTINES[SEND_FILE].c_str()) == 0){
			if(send(socketfd, START_ROUTINE)){
				std::cout << "Uh oh, send start routine error." << std::endl;
				close(socketfd);
				continue;
			}
			if(recv_file_routine(socketfd)){
				std::cout << "Uh oh, recv file routine error!" << std::endl;
				close(socketfd);
				continue;
			}
		}else if(std::strcmp(req_data.c_str(), ROUTINES[RECV_FILE].c_str()) == 0){
			if(send(socketfd, START_ROUTINE)){
				std::cout << "Uh oh, send start routine error." << std::endl;
				close(socketfd);
				continue;
			}
			/*if(send_file_routine(socketfd)){
				std::cout << "Uh oh, send file routine error!\n";
				close(socketfd);
				continue;
			}*/
		}else{
			if(send(socketfd, BAD_ROUTINE)){
				std::cout << "Uh oh, send start routine error." << std::endl;
				close(socketfd);
				continue;
			}
		}

		std::cout << "Connection #" << server.hit << " is done." << std::endl;
		close(socketfd);
	}
	// Should not end.
}
