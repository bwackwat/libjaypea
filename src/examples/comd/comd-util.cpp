#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <thread>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cryptopp/aes.h"
#include "cryptopp/osrng.h"
#include "cryptopp/modes.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "util.hpp"
#include "comd-util.hpp"

std::string IDENTITY = "This is my example identification string.";
std::string VERIFIED = "Your identity has been verified.";

std::string START_ROUTINE = "You are welcome to start the requested routine.";
std::string BAD_ROUTINE = "You have requested an invalid routine!";

std::map<enum ComdRoutine, std::string> ROUTINES = {
	{ SHELL, "I would like to use the shell please." },
	{ SEND_FILE, "I would like to send a file please." },
	{ RECV_FILE, "I would like to receive a file please." }
};
