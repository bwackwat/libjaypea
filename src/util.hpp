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
#include <fcntl.h>
#include <string.h>

#include "json.hpp"

#define CONFIG_PATH "etc/configuration.json"
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

enum RequestResult {
	HTTP,
	HTTP_API,
	API
};

class Util{
private:
	static std::vector<struct Argument> arguments;

public:
	static bool verbose;
	static JsonObject config_object;

	static void define_argument(std::string name, std::string& value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);
	static void define_argument(std::string name, int* value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);
	static void define_argument(std::string name, bool* value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);

/*
	static size_t read_size_t(const char* data);
	static void write_size_t(size_t value, char* data);
*/

	static void parse_arguments(int argc, char** argv, std::string description);

	static bool strict_compare_inequal(const char* first, const char* second, int count = -1);
	static void set_non_blocking(int fd);

	static std::string get_redirection_html(const std::string& hostname);

	static enum RequestResult parse_http_api_request(const char* request, JsonObject* request_obj);
};

