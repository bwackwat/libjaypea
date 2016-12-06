#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <sstream>

#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#define PACKET_LIMIT 2048
#define CONNECTIONS_LIMIT 2048
#define FILE_PART_LIMIT 1024

#define PRINT(msg) std::cout << msg << std::endl;
#define ERROR(msg) std::cout << "Uh oh, " << msg << " error." << std::endl;

//#define DO_DEBUG
#if defined(DO_DEBUG)
	#define DEBUG(msg) std::cout << msg << std::endl;
	#define DEBUG_SLEEP(sec) sleep(sec);
#else
	#define DEBUG(msg)
	#define DEBUG_SLEEP(sec)
#endif

bool strict_compare_inequal(const char* forst, const char* second, int count = -1);

