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

	newtermios.c_lflag &= static_cast<unsigned int>(~(ECHO | ICANON | IEXTEN | ISIG));
	newtermios.c_iflag &= static_cast<unsigned int>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
	newtermios.c_cflag &= static_cast<unsigned int>(~(CSIZE | PARENB));
	newtermios.c_cflag |= CS8;
	newtermios.c_oflag &= static_cast<unsigned int>(~(OPOST));
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
		ERROR("ttyraw")
		return 1;
	}

	memset(chars, 0, STDIN_MAX);
	while(1){
		if(!stdin_available()){
			continue;
		}

		len = read(STDIN_FILENO, &chars, STDIN_MAX);
		if(len < 0){
			ERROR("read")
			break;
		}
		if(chars[0] == 3){
			PRINT("CTRL + C!")
			break;
		}else if(chars[0] == 27 && chars[1] == 91){
			switch(chars[2]){
			case 65:
				PRINT("Up!")
				break;
			case 66:
				PRINT("Down!")
				break;
			case 67:
				PRINT("Right!")
				break;
			case 68:
				PRINT("Left!")
				break;
			default:
				print(chars);
			}
		}else{
			print(chars);
		}
		std::cout << '\r';

		if(len > 5){
			PRINT("You found a key which is represented by more than 5 characters!")
		}

		memset(chars, 0, STDIN_MAX);
	}
	
	if(ttyreset(STDIN_FILENO) < 0){
		ERROR("ttyreset")
		return 1;
	}

	return 0;
}
