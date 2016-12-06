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

#include "cryptopp/aes.h"
#include "cryptopp/osrng.h"
#include "cryptopp/modes.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

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

// These are used for authorization.
// There is basically only a single auth strategy,
// but implementing password protection is realistic.
extern std::string IDENTITY, VERIFIED;

// These are "routines" of operation.
// Shell is the classic routine,
// File transfer is new.
//extern std::string SHELL, SEND_FILE, RECV_FILE;

enum Routine {
	SHELL,
	SEND_FILE,
	RECV_FILE
};

extern std::map<enum Routine, std::string> ROUTINES;

extern std::string START_ROUTINE, BAD_ROUTINE;

extern byte* KEY;
extern byte* IV;
extern byte* SALT;
extern CryptoPP::AutoSeededRandomPool rand_tool;

int send(int fd, std::string data);
int init_crypto(std::string keyfile);
void set_non_blocking(int fd);

std::string encrypt(std::string data);
std::string decrypt(std::string data);

int send_file_routine(int socketfd, std::string file_path);
int recv_file_routine(int socketfd);
