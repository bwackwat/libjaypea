#include <iostream>
#include <string>
#include <cstring>

#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#define PORT 1010
#define PACKET_LIMIT 2048
#define CONNECTIONS_LIMIT 2048

#define PRINT(msg) std::cout << msg << std::endl;
#define ERROR(msg) std::cout << "Uh oh, " << msg << " error." << std::endl;

#define DO_DEBUG
#if defined(DO_DEBUG)
	#define DEBUG(msg) std::cout << msg << std::endl;
	#define DEBUG_SLEEP(sec) sleep(sec);
#else
	#define DEBUG(msg)
	#define DEBUG_SLEEP(sec)
#endif
