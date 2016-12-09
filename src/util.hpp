#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <sstream>
#include <vector>
#include <tuple>
#include <functional>
#include <map>

#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include "json.hpp"

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

class Util{
private:
	static std::map<std::string, std::reference_wrapper<std::string>> string_arguments;
	static std::map<std::string, int*> int_arguments;
	static std::map<std::string, bool*> bool_arguments;
	static std::map<std::string, std::vector<std::string>> argument_flags;
	static std::vector<std::string> required_arguments;

public:
	static JsonObject config;

	static void define_argument(std::string name, std::string& value, std::vector<std::string> flags = {}, bool required = false);
	static void define_argument(std::string name, int* value, std::vector<std::string> flags = {}, bool required = false);
	static void define_argument(std::string name, bool* value, std::vector<std::string> flags = {}, bool required = false);
	static void parse_arguments(int argc, char** argv, std::string description);

//	static void initialize_arguments(int argic, char** argv, std::vector<std::tuple<enum ArgumentType, std::string, void*, bool>> arguments, std::string description);

	static bool strict_compare_inequal(const char* forst, const char* second, int count = -1);
};

