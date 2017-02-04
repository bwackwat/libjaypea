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
#include "symmetric-event-client.hpp"

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
		ERROR("tcsetattr")
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
		ERROR("tcsetattr")
		return -1;
	}

	return 0;
}

int ttyreset(int fd){
	if(tcsetattr(fd, TCSAFLUSH, &old_term) < 0){
		ERROR("tcsetattr")
		return -1;
	}

	return 0;
}*/

static int ttyraw(int fd){
	if(tcgetattr(fd, &old_term) < 0){
		ERROR("tcgetattr")
		return -1;
	}
	new_term = old_term;
	new_term.c_lflag &= static_cast<unsigned int>(~(ICANON));
	if(tcsetattr(fd, TCSANOW, &new_term) < 0){
		ERROR("tcsetattr")
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

int main(int argc, char** argv){
	enum ComdState state = VERIFY_IDENTITY;
	enum ComdRoutine routine = SHELL;
	int shell_client_fd;
	ssize_t len;
	char packet[PACKET_LIMIT];

	std::string ip_address;
	bool use_ip = false;
	std::string hostname;
	int port;
	std::string keyfile;
	std::string file_path;

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("ip_address", ip_address, {"-ip"}, [&](){
		use_ip = true;});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("send-file", file_path, {"-sf"}, [&](){
		routine = SEND_FILE;});
	Util::define_argument("recv-file", file_path, {"-rf"}, [&](){
		routine = RECV_FILE;});
	Util::parse_arguments(argc, argv, "This is a secure client for comd, supporting remote shell, send file, and receive file routines.");

	SymmetricEventClient client(keyfile);

	if(!use_ip){
		client.add(new Connection(hostname, static_cast<uint16_t>(port)));
	}else if(!ip_address.empty()){
		client.add(new Connection(static_cast<uint16_t>(port), ip_address));
	}else{
		PRINT("You must provide either a hostname or an ip_address!")
		return 1;
	}

	client.on_connect = [&](int fd){
		if(client.send(fd, IDENTITY.c_str(), IDENTITY.length())){
			ERROR("client send identity")
			return true;
		}
		return false;
	};

	if(routine == SHELL){
		if(ttyraw(STDIN_FILENO) < 0){
			ttyreset(STDIN_FILENO);
			ERROR("ttyraw")
			return -1;
		}
	}

	client.run([&](int fd, const char* data, ssize_t data_length)->ssize_t{
		switch(state){
		case VERIFY_IDENTITY:
			PRINT(data)
			if(client.send(fd, ROUTINES[routine].c_str(), ROUTINES[routine].length())){
				return -1;
			}
			state = SELECT_ROUTINE;
			break;
		case SELECT_ROUTINE:
			PRINT(data)
			switch(routine){
			case SHELL:
				std::cout << "\n\r" << hostname << " > " << std::flush;

				Util::set_non_blocking(STDIN_FILENO);

				std::signal(SIGINT, sigcatch);
				std::signal(SIGQUIT, sigcatch);
				std::signal(SIGTERM, sigcatch);

				/*
				 * Major bug note:
				 * When using "fd" within the lambda, the int "fd" inside would corrupt to an
				 * apparently random number like 32767 (near the fd limit). This is some form
				 * of a complex overflow bug, due to the fact that the "fd" originally
				 * passed into the on_read std::function within tcp-event-client is coming
				 * from a class pointer!
				 *
				 * Compare that to comd where "fd" does get directly passed into the lambda,
				 * but no issues arise because it is coming from an int member data (not a
				 * contrived class pointer). The exact reason for why a class pointer's variable
				 * caused such unsung havoc is unknown, yet likely has to do with a shifting
				 * underlyign stack of pointers, and the int is accesses by referece here...
				 */
				shell_client_fd = fd;
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
						packet[len] = 0;
						if(client.send(shell_client_fd, packet, static_cast<size_t>(len))){
							return true;
						}
					}
					return false;
				};
				break;
			case SEND_FILE:
				//TODO send file name
				break;
			case RECV_FILE:
				//TODO write ok packet???
				break;
			}
			state = EXCHANGE_PACKETS;
			break;
		case EXCHANGE_PACKETS:
			std::cout << data;
			std::cout << '\r' << hostname << " > " << std::flush;
			break;
		}
		return data_length;
	});

	if(routine == SHELL){
		if(ttyreset(STDIN_FILENO) < 0){
			ERROR("ttyreset")
		}
	}

	return 0;
}
