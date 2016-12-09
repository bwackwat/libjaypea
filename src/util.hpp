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

enum ArgumentType {
	ARG_STRING,
	ARG_INTEGER,
	ARG_BOOLEAN
};

struct Argument {
	std::string name;
	std::vector<std::string> alts;
	std::function<void()> callback;
	bool required;
	enum ArgumentType type;

	std::reference_wrapper<std::string> string_value;
	int* integer_value;
	bool* boolean_value;
};

class Util{
private:
	static std::vector<struct Argument> arguments;

public:
	static JsonObject config;

	static void define_argument(std::string name, std::string& value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);
	static void define_argument(std::string name, int* value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);
	static void define_argument(std::string name, bool* value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);

	static void parse_arguments(int argc, char** argv, std::string description);

	static bool strict_compare_inequal(const char* forst, const char* second, int count = -1);
};

