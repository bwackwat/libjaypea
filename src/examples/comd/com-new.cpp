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
#include "comd-util.hpp"
#include "simple-tcp-client.hpp"

/*
void send_sigint(uint8_t s){
	DEBUG("CRTL C TO COMD")
	uint8_t byte = s;
	if(write(socketfd, &byte, 1) == -1){
		std::cout << "Uh oh, write error." << std::endl;
	}
}
*/

static bool stdin_available(){
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

static int ttyraw(int fd){
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

static int ttyreset(int fd){
	if(tcsetattr(fd, TCSANOW, &old_term) < 0){
		return -1;
	}
	return 0;
}

[[noreturn]] static void sigcatch(int sig){
	PRINT("caught " << sig << " signal!")
	ttyreset(STDIN_FILENO);
	exit(0);
}

static int shell_routine(std::string hostname, int socketfd){
	ssize_t res;
	char packet[PACKET_LIMIT];
	std::string res_data;
	
	std::cout << "\r" << hostname << " > " << std::flush;

	Util::set_non_blocking(socketfd);
	Util::set_non_blocking(STDIN_FILENO);

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

		if(!stdin_available()){
			continue;
		}
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

enum ComState{
	SENT_IDENTITY,
	SENT_ROUTINE,
	SENDING_PACKETS
};

int main(int argc, char** argv){
	ComState state = SENT_IDENTITY;
	ssize_t len;
	char packet[PACKET_LIMIT];

	std::string line;
	std::string req_data;
	std::string res_data;

	std::string hostname = "localhost";
	int port = 3424;
	std::string keyfile = "etc/keyfile";
	std::string file_path;

	enum Routine routine = SHELL;

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("send-file", file_path, {"-sf"}, [routine]()mutable{
		routine = SEND_FILE;});
	Util::define_argument("recv-file", file_path, {"-rf"}, [routine]()mutable{
		routine = RECV_FILE;});
	Util::parse_arguments(argc, argv, "This is a secure client for comd, supporting remote shell, send file, and receive file routines.");

	SymmetricEventClient client(keyfile);
	client.add(hostname, static_cast<uint16_t>(port));

	client.on_connect = [&](int fd){
		if(client.send(fd, IDENTITY.c_str(), IDENTITY.length())){
			ERROR("client send identity")
			return true;
		}
		return false;
	};

	client.run([&](int fd, const char* data, ssize_t data_length){
		switch(state){
		case SENT_IDENTITY:
			PRINT(data)
			if(client.send(fd, ROUTINES[routine].c_str(), ROUTINES[routine].length())){
				return true;
			}
			state = SENT_ROUTINE;
			break;
		case SENT_ROUTINE:
			switch(routine){
			case SHELL:
				client.on_event_loop = [&](){
					if(!stdin_available()){
						return false;
					}
					if((len = read(STDIN_FILENO, packet, PACKET_LIMIT)) < 0){
						if(errno != EWOULDBLOCK){
							ERROR("stdin read")
							return true;
						}
					}else if(len == 0){
						ERROR("stdin read zero");
						return true;
					}else{
						if(client.send(fd, packet, static_cast<size_t>(len))){
							return true;
						}
					}
					return false;
			case SEND_FILE:
				//TODO send file name
				return false;
			case RECV_FILE:
				//TODO write ok packet???
				return false;
			}
		}
		case SENDING_PACKETS:
			PRINT(data)

	// Default method, shell usage.

	if(send(client.fd, ROUTINES[routine])){
		std::cout << "Uh oh, send routine error." << std::endl;
		return 1;
	}

	packet[0] = 0;
	if((len = read(client.fd, packet, PACKET_LIMIT)) < 0){
		std::cout << "Uh oh, read method error." << std::endl;
		return 1;
	}
	packet[len] = 0;
	res_data = decrypt(packet);
	std::cout << res_data << std::endl;

	// Handshake complete !

	if(routine == SEND_FILE){
		if(send_file_routine(client.fd, file_path)){
			std::cout << "Uh oh, send file routine error!" << std::endl;
			return 1;
		}
	}else{
		if(shell_routine(hostname, client.fd)){
			std::cout << "Uh oh, shell routine error!" << std::endl;
			return 1;
		}
	}

	return 0;
}
