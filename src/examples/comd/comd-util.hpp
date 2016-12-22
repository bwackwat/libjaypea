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

// These are used for authorization.
// There is basically only a single auth strategy,
// but implementing password protection is realistic.
extern std::string IDENTITY, VERIFIED;

// These are "routines" of operation.
// Shell is the classic routine,
// File transfer is new.
//extern std::string SHELL, SEND_FILE, RECV_FILE;

enum ComdRoutine {
	SHELL,
	SEND_FILE,
	RECV_FILE
};

enum ComdState {
	VERIFY_IDENTITY,
	SELECT_ROUTINE,
	EXCHANGE_PACKETS
};

extern std::map<enum ComdRoutine, std::string> ROUTINES;

extern std::string START_ROUTINE, BAD_ROUTINE;
