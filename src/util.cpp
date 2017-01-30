#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <thread>
#include <ctime>
#include <tuple>
#include <vector>
#include <algorithm>
#include <csignal>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <execinfo.h>

#include "util.hpp"

std::vector<struct Argument> Util::arguments;

bool Util::verbose;
JsonObject Util::config_object;

void Util::define_argument(std::string name, std::string& value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	PRINT("DEFAULT SET " << name << " = " << value)
	arguments.push_back({name, alts, callback, required, ARG_STRING, std::ref(value), 0, 0});
}

void Util::define_argument(std::string name, int* value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	PRINT("DEFAULT SET " << name << " = " << *value)
	arguments.push_back({name, alts, callback, required, ARG_INTEGER, std::ref(name), value, 0});
}

void Util::define_argument(std::string name, bool* value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	PRINT("DEFAULT SET " << name << " = " << *value)
	arguments.push_back({name, alts, callback, required, ARG_BOOLEAN, std::ref(name), 0, value});
}

static std::string get_exe_path()
{
	char result[128];
	ssize_t count = readlink( "/proc/self/exe", result, 128);
	return std::string(result, (count > 0) ? static_cast<size_t>(count) : 0);
}

[[noreturn]] static void trace_segfault(int sig){
	void *array[10];
	int size = backtrace(array, 10);

	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
}

uint32_t Util::read_uint32_t(const char* data){
	return static_cast<uint32_t>(data[0]) << 24 |
		static_cast<uint32_t>(data[1]) << 16 |
		static_cast<uint32_t>(data[2]) << 8 |
		static_cast<uint32_t>(data[3]);
}

void Util::write_uint32_t(uint32_t value, char* data){
	data[0] = static_cast<char>(value);
	data[1] = static_cast<char>(value) >> 8;
	data[2] = static_cast<char>(value) >> 16;
	data[3] = static_cast<char>(value) >> 24;
}

void Util::parse_arguments(int argc, char** argv, std::string description){
	define_argument("verbose", &verbose, {"-v"});

	std::signal(SIGSEGV, trace_segfault);

	PRINT("------------------------------------------------------")

	std::string check;
	std::cout << "Arguments: ";
	for(int i = 0; i < argc; ++i){
		std::cout << argv[i] << ' ';
	}
	std::cout << '\n';
	PRINT("std::thread::hardware_concurrency = " << std::thread::hardware_concurrency())

	PRINT("------------------------------------------------------")

	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "--help") == 0 ||
		std::strcmp(argv[i], "-h") == 0){
			PRINT("Usage: " << argv[0])
			PRINT("     --help")
			PRINT("  OR -h\n")
			for(auto& arg : arguments){
				if(arg.required){
					PRINT("     REQUIRED")
				}
				switch(arg.type){
				case ARG_STRING:
					PRINT("     --" << arg.name << " <string>")
					for(auto& alt : arg.alts){
						PRINT("  OR " << alt << " <string>")
					}
					PRINT("     (Default value: " << arg.string_value.get() << ")\n")
					break;
				case ARG_INTEGER:
					PRINT("     --" << arg.name << " <integer>")
					for(auto& alt : arg.alts){
						PRINT("  OR " << alt << " <integer")
					}
					PRINT("     (Default value: " << *arg.integer_value << ")\n")
					break;
				case ARG_BOOLEAN:
					PRINT("     --" << arg.name)
					for(auto& alt : arg.alts){
						PRINT("  OR " << alt)
					}
					PRINT("     (Disabled by default.)\n")
					break;
				}
			}
			PRINT(description)
			exit(0);
		}
	}

	std::string root_path = get_exe_path();
	root_path = root_path.substr(0, root_path.find_last_of('/'));
	root_path = root_path.substr(0, root_path.find_last_of('/')) + '/';
	std::string config_path = root_path + CONFIG_PATH;
	std::ifstream config_file(config_path);
	if(config_file.is_open()){
		std::string config_data((std::istreambuf_iterator<char>(config_file)),
			(std::istreambuf_iterator<char>()));
		config_object.parse(config_data.c_str());
		PRINT("Loaded " << config_path)
		// PRINT(config_object.stringify(true))
		for(auto& arg : arguments){
			if(config_object.objectValues.count(arg.name)){
				switch(arg.type){
				case ARG_STRING:
					arg.string_value.get() = config_object[arg.name]->stringValue;
					PRINT("CONFIG SET STRING " << arg.name << " = " << arg.string_value.get())
					break;
				case ARG_INTEGER:
					*arg.integer_value = std::stoi(config_object[arg.name]->stringValue);
					PRINT("CONFIG SET INTEGER " << arg.name << " = " << *arg.integer_value)
					break;
				case ARG_BOOLEAN:
					*arg.boolean_value = true;
					PRINT("CONFIG SET BOOLEAN " << arg.name << " = " << *arg.boolean_value)
					break;
				}
				if(arg.callback != nullptr){
					arg.callback();
				}
			}
		}
	}else{
		PRINT("Could not open " << config_path << ", ignoring.")
	}
	
	PRINT("------------------------------------------------------")

	for(int i = 0; i < argc; ++i){
		for(auto& arg: arguments){
			check = "--" + arg.name;
			std::function<bool(const char*)> check_lambda;
			switch(arg.type){
			case ARG_STRING:
				check_lambda = [argc, i, arg, argv](const char* check_sub) mutable -> bool{
					if(std::strcmp(argv[i], check_sub) == 0){
						if(argc > i + 1){
							arg.string_value.get() = std::string(argv[i + 1]);
							PRINT("ARG SET " << arg.name << " = " << arg.string_value.get())
							if(arg.callback != nullptr){
								arg.callback();
							}
						}else{
							PRINT("No string provided for " << check_sub)
						}
						return true;
					}
					return false;
				};
				break;
			case ARG_INTEGER:
				check_lambda = [argc, i, arg, argv](const char* check_sub) mutable -> bool{
					if(std::strcmp(argv[i], check_sub) == 0){
						if(argc > i + 1){
							*arg.integer_value = std::stoi(argv[i + 1]);
							PRINT("ARG SET " << arg.name << " = " << *arg.integer_value)
							if(arg.callback != nullptr){
								arg.callback();
							}
						}else{
							PRINT("No integer provided for " << check_sub)
						}
						return true;
					}
					return false;
				};
				break;
			case ARG_BOOLEAN:
				check_lambda = [argc, i, arg, argv](const char* check_sub) mutable -> bool{
					if(std::strcmp(argv[i], check_sub) == 0){
						*arg.boolean_value = true;
						PRINT("ARG SET " << arg.name << " = " << *arg.boolean_value)
						if(arg.callback != nullptr){
							arg.callback();
						}
						return true;
					}
					return false;
				};
				break;
			}
			if(check_lambda(check.c_str()))break;
			for(auto& alt : arg.alts){
				if(check_lambda(alt.c_str()))break;
			}
		}	
	}

	PRINT("------------------------------------------------------")
}

/*
 * Returns true if the c-strings are strictly inequal.
 */
bool Util::strict_compare_inequal(const char* first, const char* second, int count){
	if(count){
		for(int i = 0; i < count; ++i){
			if(first[i] != second[i]){
				return true;
			}
		}
	}else{
		for(int i = 0;; ++i){
			if(first[i] != second[i] ||
			first[i] + second[i] == first[i] || 
			first[i] + second[i] == second[i]){
				return true;
			}
		}
	}
	return false;
}

void Util::set_non_blocking(int fd){
	int flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
}

std::string Util::get_redirection_html(const std::string& hostname){
	std::string response_body = "<!doctype html><html>\n"
		"<head>\n"
		"<title>301 Moved Permanently</title>\n"
		"</head>\n"
		"<body>\n"
		"<h1>301 Moved Permanently</h1>\n"
		"<p>This page has permanently moved to <a href=\"https://" +
		hostname +
		"/\">https://" +
		hostname +
		"/</a>.</p>\n"
		"</body>\n"
		"</html>";
	std::string response = "HTTP/1.1 301 Moved Permanently\n" 
		"Location: https://" + hostname + "/\n"
		"Accept-Ranges: bytes\n"
		"Content-Type: text/html\n"
		"Content-Length: " + std::to_string(response_body.length()) + "\n"
		"\r\n\r\n" +
		response_body;
	return response;
}

enum RequestResult Util::parse_http_api_request(const char* request, JsonObject* request_obj){
	const char* it = request;
	bool exit_http_parse = false;

	request_obj->objectValues["method"] = new JsonObject("GET");
	request_obj->objectValues["route"] = new JsonObject(STRING);
	request_obj->objectValues["protocol"] = new JsonObject("JPON");

	if(!strict_compare_inequal(request, "GET /", 5)){
		it += 5;
	}else if(!strict_compare_inequal(request, "POST /", 6)){
		request_obj->objectValues["method"]->stringValue = "POST";
		it += 6;
	}else if(!strict_compare_inequal(request, "PUT /", 5)){
		request_obj->objectValues["method"]->stringValue = "PUT";
		it += 5;
	}else{
		request_obj->parse(it);
		// Your HTTP is just getting ignored.
		return API;
	}
	
	int state = 1;
	std::string new_key = "";
	std::string new_value = "/";

	/*
		1 = get route
		2 = get route parameter key
		3 = get route parameter value
		4 = get protocol
		5 = get header key
		6 = get header value
		7+ = finding newlines and carriage returns
		8 = found \r\n\r\n, if api route, will parse json in request body
	*/

	for(; *it; ++it){
		if(exit_http_parse){
			break;
		}
		switch(*it){
		case ' ':
			if(state == 1){
				if(new_value.length() == 0){
					exit_http_parse = true;
				}else{
					request_obj->objectValues["route"]->stringValue = new_value;
					state = 4;
					new_value = "";
				}
				continue;
			}else if(state == 2){
				state = 4;
				continue;
			}else if(state == 3){
				if(!request_obj->objectValues.count(new_key)){
					request_obj->objectValues[new_key] = new JsonObject();
				}
				request_obj->objectValues[new_key]->type = NOTYPE;
				request_obj->objectValues[new_key]->parse(new_value.c_str());
				if(request_obj->objectValues[new_key]->type == NOTYPE){
					request_obj->objectValues[new_key]->type = STRING;
					request_obj->objectValues[new_key]->stringValue = new_value;
				}
				state = 4;
				new_value = "";
				continue;
			}
			break;
		case '?':
			if(state == 1){
				request_obj->objectValues["route"]->stringValue = new_value;
				new_key = "";
				state = 2;
				continue;
			}
			break;
		case '=':
			if(state == 2){
				new_value = "";
				state = 3;
				continue;
			}
			break;
		case '&':
			if(state == 3){
				if(!request_obj->objectValues.count(new_key)){
					request_obj->objectValues[new_key] = new JsonObject();
				}
				request_obj->objectValues[new_key]->type = NOTYPE;
				request_obj->objectValues[new_key]->parse(new_value.c_str());
				if(request_obj->objectValues[new_key]->type == NOTYPE){
					request_obj->objectValues[new_key]->type = STRING;
					request_obj->objectValues[new_key]->stringValue = new_value;
				}
				new_key = "";
				state = 2;
				continue;
			}
			break;
		case '\n':
			if(state == 4){
				request_obj->objectValues["protocol"]->stringValue = new_value;
				new_key = "";
				state = 5;
				continue;
			}else if(state == 5){
				state = 7;
			}else if(state == 6){
				if(!request_obj->objectValues.count(new_key)){
					request_obj->objectValues[new_key] = new JsonObject();
				}
				request_obj->objectValues[new_key]->type = STRING;
				request_obj->objectValues[new_key]->stringValue = new_value;
				new_key = "";
				state = 5;
				continue;
			}
			break;
		case ':':
			if(state == 5){
				++it;
				new_value = "";
				state = 6;
				continue;
			}
			break;
		}
		switch(state){
		case 0:
		case 1:
		case 3:
		case 4:
		case 6:
			if(*it == '%' && *(it + 1) == '2' && *(it + 1) == '2'){
				it++;
				it++;
				new_value += '"';
			}else{
				if(*it != '\r'){
					new_value += *it;
				}
			}
			break;
		case 2:
		case 5:
			new_key += *it;
			break;
		default:
			if(*it == '\r' || *it == '\n'){
				state++;
			}
			if(state >= 8){
				exit_http_parse = true;
			}
		}
	}

	if(request_obj->objectValues["route"]->stringValue.length() >= 4 &&
	request_obj->objectValues["route"]->stringValue.substr(0, 4) == "/api"){
		request_obj->parse(it);
		return HTTP_API;
	}

	return HTTP;
}
