#include <string>
#include <cstring>
#include <iostream>
#include <csignal>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.hpp"

int socketfd;

void send_sigint(uint8_t s){
	DEBUG("CRTL C TO COMD")
	uint8_t byte = s;
	if(write(socketfd, &byte, 1) == -1){
		std::cout << "Uh oh, write error." << std::endl;
	}
}

bool stdin_available(){
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	select(STDIN_FILENO + 1, &fds, 0, 0, &tv);
	return (FD_ISSET(0, &fds));
}

static struct termios old_term, new_term;

/*int ttyraw(int fd){
	if(tcgetattr(fd, &old_term) < 0){
		std::cout << "Uh oh, tcsetattr error!" << std::endl;
		return -1;
	}
	new_term = old_term;

	new_term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	new_term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	new_term.c_cflag &= ~(CSIZE | PARENB);
	new_term.c_cflag |= CS8;
	new_term.c_oflag &= ~(OPOST);
	new_term.c_cc[VMIN] = 1;
	new_term.c_cc[VTIME] = 0;

	if(tcsetattr(fd, TCSAFLUSH, &new_term) < 0){
		std::cout << "Uh oh, tcsetattr error!" << std::endl;
		return -1;
	}

	return 0;
}

int ttyreset(int fd){
	if(tcsetattr(fd, TCSAFLUSH, &old_term) < 0){
		std::cout << "Uh oh, tcsetattr error!" << std::endl;
		return -1;
	}

	return 0;
}*/

int ttyraw(int fd){
	if(tcgetattr(fd, &old_term) < 0){
		std::cout << "Uh oh, tcgetattr error!" << std::endl;
		return -1;
	}
	new_term = old_term;
	new_term.c_lflag &= ~(ICANON);
	if(tcsetattr(fd, TCSANOW, &new_term) < 0){
		std::cout << "Uh oh, tcsetattr error!" << std::endl;
		return -1;
	}

	return 0;
}

int ttyreset(int fd){
	if(tcsetattr(fd, TCSANOW, &old_term) < 0){
		return -1;
	}
	return 0;
}

void sigcatch(int sig){
	DEBUG("caught" << sig << " signal!")
	ttyreset(STDIN_FILENO);
	exit(0);
}

std::string hostname = "localhost";

int shell_routine(int socketfd){
	ssize_t res;
	char packet[PACKET_LIMIT];
	std::string res_data;
	
	std::cout << "\r" << hostname << " > " << std::flush;

	set_non_blocking(socketfd);
	set_non_blocking(STDIN_FILENO);

	std::signal(SIGINT, sigcatch);
	std::signal(SIGQUIT, sigcatch);
	std::signal(SIGTERM, sigcatch);

	if(ttyraw(STDIN_FILENO) < 0){
		ttyreset(STDIN_FILENO);
		std::cout << "Uh oh, ttyraw error!" << std::endl;
		return 1;
	}

	while(1){
		packet[0] = 0;
		if((res = read(socketfd, packet, PACKET_LIMIT)) < 0){
			DEBUG("Socket read nonblocking.");
			DEBUG_SLEEP(1)
		}else if(res == 0){
			DEBUG("Socket no read. Done.");
			break;
		}else{
			packet[res] = 0;
			try{
				res_data = decrypt(std::string(packet));
			}
			catch(const CryptoPP::Exception& e){
				std::cout << "Implied hacker! " << e.what() <<std::endl;
			}

			std::cout << res_data;
			std::cout << "\r" << hostname << " > " << std::flush;
		}

		//if(!stdin_available()){
		//	continue;
		//}
		packet[0] = 0;
		if((res = read(STDIN_FILENO, packet, PACKET_LIMIT)) < 0){
			DEBUG("Stdin read nonblocking.");
			sleep(1);
		}else if(res == 0){
			DEBUG("Stdin no read. Done.");
			break;
		}else{
			packet[res] = 0;
			res_data = encrypt(std::string(packet));
			if((res = write(socketfd, res_data.c_str(), static_cast<size_t>(res_data.length()))) < 0){
				std::cout << "Uh oh, write error." << std::endl;
				break;
			}
			DEBUG("Socket write:" << packet << "|" << res)
		}
	}

	if(ttyreset(STDIN_FILENO) < 0){
		std::cout << "Uh oh, ttyreset error!" << std::endl;
		return 1;
	}

	return 0;
}

int main(int argc, char** argv){
	int res;
	ssize_t len;
	struct hostent *host;	
	struct sockaddr_in serv_addr;
	char packet[PACKET_LIMIT];

	std::string line;
	std::string req_data;
	std::string res_data;

	std::string hostname = "localhost";
	uint16_t port = 3424;
	std::string keyfile = "etc/keyfile";
	std::string file_path;
	enum Routine routine = SHELL;

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
		if((std::strcmp(argv[i], "-sf") == 0 ||
		std::strcmp(argv[i], "--send-file") == 0) && argc > i){
			file_path = argv[i + 1];
			routine = SEND_FILE;
		}
		if((std::strcmp(argv[i], "-rf") == 0 ||
		std::strcmp(argv[i], "--recv-file") == 0) && argc > i){
			file_path = argv[i + 1];
			routine = RECV_FILE;
		}
		if((std::strcmp(argv[i], "-?") == 0 ||
		std::strcmp(argv[i], "--help") == 0) && argc > i){
			std::cout << "Usage: --port <port> --hostname <hostname> --keyfile <keyfile>\n" << std::endl;
			return 0;
		}
	}

	if(init_crypto(keyfile)){
		return 1;
	}

	if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		std::cout << "System call to socket failed!\n" << socketfd << std::endl;
		return 1;
	}

	if((host = gethostbyname(hostname.c_str())) == 0){
		std::cout << "System call to gethostbyname failed!\n";
		std::cout << hostname << std::endl;
		return 1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = *reinterpret_cast<struct in_addr *>(host->h_addr);
	serv_addr.sin_port = htons(port);
	bzero(&(serv_addr.sin_zero), 8);

	if((res = connect(socketfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr))) < 0){
		std::cout << "System call to connect failed!\n" << res << errno << std::endl;
		return 1;
	}

	// Handshake step 1:
	// Identify yourself!
	// No password protection; basically verifying the cypher.

	if(send(socketfd, IDENTITY)){
		std::cout << "Uh oh, send identity error." << std::endl;
		return 1;
	}

	packet[0] = 0;
	if((len = read(socketfd, packet, PACKET_LIMIT)) < 0){
		std::cout << "Uh oh, read verification error." << std::endl;
		return 1;
	}
	packet[len] = 0;
	res_data = decrypt(packet);
	std::cout << res_data << std::endl;

	// Handshake step 2:
	// Select a method!
	// Default method, shell usage.

	if(send(socketfd, ROUTINES[routine])){
		std::cout << "Uh oh, send routine error." << std::endl;
		return 1;
	}

	packet[0] = 0;
	if((len = read(socketfd, packet, PACKET_LIMIT)) < 0){
		std::cout << "Uh oh, read method error." << std::endl;
		return 1;
	}
	packet[len] = 0;
	res_data = decrypt(packet);
	std::cout << res_data << std::endl;

	// Handshake complete !

	if(routine == SEND_FILE){
		if(send_file_routine(socketfd, file_path)){
			std::cout << "Uh oh, send file routine error!" << std::endl;
			return 1;
		}
	}else{
		if(shell_routine(socketfd)){
			std::cout << "Uh oh, shell routine error!" << std::endl;
			return 1;
		}
	}

	return 0;
}
