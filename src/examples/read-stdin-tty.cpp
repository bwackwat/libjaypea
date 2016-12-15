// http://www.minek.com/files/unix_examples/raw.html

#include <cstring>
#include <iostream>
#include <csignal>

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>

#include "util.hpp"

#define STDIN_MAX 10

static struct termios oldtermios;

static int ttyraw(int fd){
	struct termios newtermios;
	if(tcgetattr(fd, &oldtermios) < 0){
		return -1;
	}
	newtermios = oldtermios;

	newtermios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	newtermios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	newtermios.c_cflag &= ~(CSIZE | PARENB);
	newtermios.c_cflag |= CS8;
	newtermios.c_oflag &= ~(OPOST);
	newtermios.c_cc[VMIN] = 1;
	newtermios.c_cc[VTIME] = 0;

	if(tcsetattr(fd, TCSAFLUSH, &newtermios) < 0){
		return -1;
	}

	return 0;
}

static int ttyreset(int fd){
	if(tcsetattr(fd, TCSAFLUSH, &oldtermios) < 0){
		return -1;
	}

	return 0;
}

[[noreturn]] static void sigcatch(int sig){
	PRINT("caught " << sig << " signal!")
	ttyreset(STDIN_FILENO);
	exit(0);
}

static bool stdin_available(){
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	select(STDIN_FILENO + 1, &fds, 0, 0, &tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}

static void print(char* chars){
	char* next = chars;
	while(*next){
		std::cout << static_cast<int>(*next) << std::endl << '\r';
		next++;
	}
	std::cout << std::endl;
}

int main(int argc, char** argv){
	ssize_t len;
	char chars[STDIN_MAX];

	Util::parse_arguments(argc, argv, "This program outputs raw TTY character bytes. Some characters are represented by multiple bytes (multiple stdin chars.)");

	std::signal(SIGINT, sigcatch);
	std::signal(SIGQUIT, sigcatch);
	std::signal(SIGTERM, sigcatch);

	if(ttyraw(STDIN_FILENO) < 0){
		std::cout << "Uh oh, ttyraw error!" << std::endl;
		return 1;
	}

	memset(chars, 0, STDIN_MAX);
	while(1){
		if(!stdin_available()){
			continue;
		}

		len = read(STDIN_FILENO, &chars, STDIN_MAX);
		if(len < 0){
			std::cout << "Uh oh, read error!" << std::endl;
			break;
		}
		if(chars[0] == 3){
			std::cout << "CTRL + C!" << std::endl;
			break;
		}else if(chars[0] == 27 && chars[1] == 91){
			switch(chars[2]){
			case 65:
				std::cout << "Up!" << std::endl;
				break;
			case 66:
				std::cout << "Down!" << std::endl;
				break;
			case 67:
				std::cout << "Right!" << std::endl;
				break;
			case 68:
				std::cout << "Left!" << std::endl;
				break;
			default:
				print(chars);
			}
		}else{
			print(chars);
		}
		std::cout << '\r';

		if(len > 5){
			std::cout << "You found a key which is represented by more than 5 characters!" << std::endl;
		}

		memset(chars, 0, STDIN_MAX);
	}
	
	if(ttyreset(STDIN_FILENO) < 0){
		std::cout << "Uh oh, ttyreset error!\n";
		return 1;
	}

	return 0;
}
