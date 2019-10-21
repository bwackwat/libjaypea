#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <sstream>
#include <vector>
#include <queue>
#include <tuple>
#include <functional>
#include <map>
#include <bitset>

#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#include "cryptopp/sha.h"
#include "cryptopp/filters.h"
#include "cryptopp/base64.h"

#include "json.hpp"
#include "distributed-node.hpp"
#include "tls-client-manager.hpp"
#include "argon2.h"

#define PACKET_LIMIT 16384
#define CONNECTIONS_LIMIT 2048
#define FILE_PART_LIMIT 1024

#define PRINT(msg) std::cout << msg << std::endl;
#define ERROR(msg) std::cerr << "Uh oh, " << msg << " error." << std::endl;

#if defined(DO_DEBUG)
	static std::mutex debug_msg_mutex;
	#define DEBUG(msg) debug_msg_mutex.lock(); std::cout << msg << std::endl; debug_msg_mutex.unlock();
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
	bool set;
	enum ArgumentType type;

	std::reference_wrapper<std::string> string_value;
	int* integer_value;
	bool* boolean_value;
};

// If we can't determine HTTP, then it's a pure JSON API request.
enum RequestResult {
	HTTP,
	HTTP_API,
	HTTP_FORM,
	API
};

class Util{
private:
	static std::vector<struct Argument*> arguments;

public:
	static bool verbose;
	static std::string config_path;
	static std::string libjaypea_path;
	static JsonObject config_object;
	static TlsClientManager https_client;
	static std::string secret_captcha_url;

	static void define_argument(std::string name, std::string& value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);
	static void define_argument(std::string name, int* value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);
	static void define_argument(std::string name, bool* value, std::vector<std::string> alts = {}, std::function<void()> callback = nullptr, bool required = false);

	static void write_file(std::string filename, std::string content);
	static std::string read_file(std::string filename);
	static std::string exec_and_wait(const char* cmd);
	
	static void parse_arguments(int argc, char** argv, std::string description);

	static bool strict_compare_inequal(const char* first, const char* second, int count = -1);
	static void set_non_blocking(int fd);
	static void set_blocking(int fd);
	static std::string url_encode(std::string str);
	static std::string url_decode(std::string str);
	static void replace_all(std::string& source, const std::string& from, const std::string& to); 

	static std::string get_redirection_html(const std::string& hostname);
	static bool endsWith(const std::string& str, const std::string& suffix);
	static bool startsWith(const std::string& str, const std::string& prefix);

	static enum RequestResult parse_http_request(const char* request, JsonObject* request_obj);

	static std::string sha256_hash(std::string data);
	static std::string hash_value_argon2d(std::string password);

	static void print_bits(const char* data, size_t data_length);
	static void clean();
};
