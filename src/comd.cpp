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

int strict_compare(const char* first, const char* second, int count){
	for(int i = 0; i < count; ++i){
		if(first[i] != second[i]){
			return 1;
		}
	}
	return 0;
}

int shell_routine(int socketfd){
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
		set_non_blocking(shell_pipe[1][0]);
		// Reading from client doesn't block (no input).
		set_non_blocking(socketfd);

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
	int listenfd, socketfd, hit;
	ssize_t res;
	struct sockaddr_in cli_addr;
	struct sockaddr_in serv_addr;
	socklen_t length = sizeof(cli_addr);
	char packet[PACKET_LIMIT];

	std::string req_data;
	std::string response;
	std::string res_data;

	std::string hostname = "localhost";
	uint16_t port = 3424;
	std::string keyfile = "etc/keyfile";

	for(int i = 0; i < argc; i++){
		if((std::strcmp(argv[i], "-p") == 0 ||
		std::strcmp(argv[i], "--port") == 0) && argc > i){
			port = static_cast<uint16_t>(std::stoi(argv[i + 1]));
		}
		if((std::strcmp(argv[i], "-h") == 0 ||
		std::strcmp(argv[i], "--hostname") == 0) && argc > i){
			hostname = argv[i + 1];
		}
		if((std::strcmp(argv[i], "-k") == 0 ||
		std::strcmp(argv[i], "--keyfile") == 0) && argc > i){
			keyfile = argv[i + 1];
		}
		if((std::strcmp(argv[i], "-?") == 0 ||
		std::strcmp(argv[i], "--help") == 0) && argc > i){
			std::cout << "Usage: --port <port> --hostname <hostname> --keyfile <keyfile>" << std::endl;
			return 0;
		}
	}

	if(init_crypto(keyfile)){
		return 1;
	}

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		std::cout << "System call to socket failed!" << std::endl << listenfd;
		return 1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if((hit = bind(listenfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr))) < 0){
		std::cout << "System call to bind failed!" << std::endl << hit << errno;
		return 1;
	}

	if(listen(listenfd, 0) < 0){
		std::cout << "System call to listen failed!" << std::endl;
		return 1;
	}

	std::cout << "comd running on port " << port << "." << std::endl;

	for(hit = 1;; hit++){
		// Easy DDOS protection. (This is not a multi-user tool.)
		if(hit != 1){
			sleep(10);
		}
		std::cout << "Accepting a new connection." << std::endl;
		if((socketfd = accept(listenfd, reinterpret_cast<struct sockaddr*>(&cli_addr), &length)) < 0){
			std::cout << "System call to accept failed!" << std::endl;
			return 1;
		}

		std::cout << "comd connection #" << hit << " engaged." << std::endl;

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

		std::cout << "Connection #" << hit << " is done." << std::endl;
		close(socketfd);
	}
	// Should not end.
}
