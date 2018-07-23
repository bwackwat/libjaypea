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

#include "argon2.h"

std::vector<struct Argument*> Util::arguments;

bool Util::verbose = false;
std::string Util::config_path = "extras/configuration.json";
std::string Util::libjaypea_path;
JsonObject Util::config_object;

//std::string Util::distribution_keyfile = std::string();
//std::string Util::distribution_start_ip_address = std::string();
//int Util::distribution_start_port = 0;
//DistributedNode* Util::distribution_node = 0;

void Util::define_argument(std::string name, std::string& value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	arguments.emplace_back(new struct Argument({name, alts, callback, required, false, ARG_STRING, std::ref(value), 0, 0}));
}

void Util::define_argument(std::string name, int* value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	arguments.emplace_back(new struct Argument({name, alts, callback, required, false, ARG_INTEGER, std::ref(name), value, 0}));
}

void Util::define_argument(std::string name, bool* value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	arguments.emplace_back(new struct Argument({name, alts, callback, required, false, ARG_BOOLEAN, std::ref(name), 0, value}));
}

static std::string get_exe_path(){
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

/*
size_t Util::read_size_t(const char* data){
	return static_cast<size_t>(
		static_cast<unsigned char>(data[0]) +
		(static_cast<unsigned char>(data[1]) << 8) + 
		(static_cast<unsigned char>(data[2]) << 16) + 
		(static_cast<unsigned char>(data[3]) << 24));
}

void Util::write_size_t(size_t value, char* data){
	data[0] = static_cast<unsigned char>(value);
	data[1] = static_cast<unsigned char>(value) >> 8;
	data[2] = static_cast<unsigned char>(value) >> 16;
	data[3] = static_cast<unsigned char>(value) >> 24;
}
*/

void Util::write_file(std::string filename, std::string content){
	std::ofstream file(filename);
	if(!file.is_open()){
		throw std::runtime_error("Could not write " + filename);
	}
	file << content;
	file.close();
}

std::string Util::read_file(std::string filename){
	std::fstream file(filename.c_str(), std::ios::binary | std::ios::in);
	if(!file){
		throw std::runtime_error("Could not read " + filename);
	}
	std::string data;
	file >> data;
	file.close();
	return data;
}

std::string Util::exec_and_wait(const char* cmd){
	std::array<char, 128> buffer;
	std::string result;
	std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if (!pipe) throw std::runtime_error("popen() failed!");
	while (!feof(pipe.get())) {
	if (fgets(buffer.data(), 128, pipe.get()) != NULL)
	    result += buffer.data();
	}
	return result;
}

void Util::parse_arguments(int argc, char** argv, std::string description){
	define_argument("verbose", &verbose, {"-v"});
	define_argument("configuration_file", config_path, {"-cf"});
	//define_argument("distribution_keyfile", distribution_keyfile, {"-dk"});
	//define_argument("distribution_start_ip_address", distribution_start_ip_address, {"-dsip"});
	//define_argument("distribution_start_port", &distribution_start_port, {"-dsp"});
	
	PRINT("------------------------------------------------------")

	libjaypea_path = get_exe_path();
	libjaypea_path = libjaypea_path.substr(0, libjaypea_path.find_last_of('/'));
	libjaypea_path = libjaypea_path.substr(0, libjaypea_path.find_last_of('/')) + '/';
	PRINT("libjaypea: " << libjaypea_path)

	std::signal(SIGSEGV, trace_segfault);

	std::string check;
	std::cout << "Arguments: ";
	for(int i = 0; i < argc; ++i){
		std::cout << argv[i] << ' ';
	}
	std::cout << '\n';
	PRINT("std::thread::hardware_concurrency: " << std::thread::hardware_concurrency())

	PRINT("------------------------------------------------------")

	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "--help") == 0 ||
		std::strcmp(argv[i], "-h") == 0){
			PRINT("Usage: " << argv[0])
			PRINT("     --help")
			PRINT("  OR -h\n")
			for(auto arg : arguments){
				if(arg->required){
					PRINT("     REQUIRED")
				}
				switch(arg->type){
				case ARG_STRING:
					PRINT("     --" << arg->name << " <string>")
					for(auto& alt : arg->alts){
						PRINT("  OR " << alt << " <string>")
					}
					PRINT("     (Default value: " << arg->string_value.get() << ")\n")
					break;
				case ARG_INTEGER:
					PRINT("     --" << arg->name << " <integer>")
					for(auto& alt : arg->alts){
						PRINT("  OR " << alt << " <integer>")
					}
					PRINT("     (Default value: " << *arg->integer_value << ")\n")
					break;
				case ARG_BOOLEAN:
					PRINT("     --" << arg->name)
					for(auto& alt : arg->alts){
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

	for(int i = 0; i < argc; ++i){
		for(auto arg : arguments){
			check = "--" + arg->name;
			std::function<bool(const char*)> check_lambda;
			switch(arg->type){
			case ARG_STRING:
				check_lambda = [argc, i, arg, argv](const char* check_sub) mutable -> bool{
					if(std::strcmp(argv[i], check_sub) == 0){
						if(argc > i + 1){
							arg->string_value.get() = std::string(argv[i + 1]);
							arg->set = true;
							PRINT("ARGUMENT SET " << arg->name << " = " << arg->string_value.get())
							if(arg->callback != nullptr){
								arg->callback();	
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
							*arg->integer_value = std::stoi(argv[i + 1]);
							arg->set = true;
							PRINT("ARGUMENT SET " << arg->name << " = " << *arg->integer_value)
							if(arg->callback != nullptr){
								arg->callback();
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
						*arg->boolean_value = true;
						arg->set = true;
						PRINT("ARGUMENT SET " << arg->name << " = " << *arg->boolean_value)
						if(arg->callback != nullptr){
							arg->callback();
						}
						return true;
					}
					return false;
				};
				break;
			}

			if(check_lambda(check.c_str()))break;
			for(auto& alt : arg->alts){
				if(check_lambda(alt.c_str()))break;
			}
		}	
	}

	PRINT("------------------------------------------------------")

	std::ifstream config_file(config_path);
	if(config_file.is_open()){
		std::string config_data((std::istreambuf_iterator<char>(config_file)),
			(std::istreambuf_iterator<char>()));
		config_object.parse(config_data.c_str());
		PRINT("Loaded " << config_path)
		// PRINT(config_object.stringify(true))
		for(auto& arg : arguments){
			if(config_object.objectValues.count(arg->name)){
				if(arg->set){
					PRINT(arg->name << " was set from the command line.")
					continue;
				}
				switch(arg->type){
				case ARG_STRING:
					arg->string_value.get() = config_object[arg->name]->stringValue;
					PRINT(arg->name << " = " << arg->string_value.get())
					break;
				case ARG_INTEGER:
					*arg->integer_value = std::stoi(config_object[arg->name]->stringValue);
					PRINT(arg->name << " = " << *arg->integer_value)
					break;
				case ARG_BOOLEAN:
					*arg->boolean_value = true;
					PRINT(arg->name << " = " << *arg->boolean_value)
					break;
				}
				if(arg->callback != nullptr){
					arg->callback();
				}
			}
		}
	}else{
		PRINT("Could not open " << config_path << ", ignoring.")
	}

	
	PRINT("------------------------------------------------------")
	
	/*distribution_node = new DistributedNode(distribution_keyfile);

	if(!distribution_start_ip_address.empty() &&
	distribution_start_port != 0){
		distribution_node->add_client(distribution_start_ip_address.c_str(), static_cast<uint16_t>(distribution_start_port));
	}*/
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

void Util::set_blocking(int fd){
	int flags = fcntl(fd, F_GETFL, 0);
	flags &= (~O_NONBLOCK);
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
		"\">https://" +
		hostname + 
		"</a>.</p>\n"
		"</body>\n"
		"</html>";
	std::string response = "HTTP/1.1 301 Moved Permanently\n" 
		"Location: https://" + hostname + "\n"
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
	
	//DEBUG("REQUEST|" << request << "|END")

	request_obj->objectValues["method"] = new JsonObject("GET");
	request_obj->objectValues["route"] = new JsonObject(STRING);
	request_obj->objectValues["protocol"] = new JsonObject("JPON");

	if(!strict_compare_inequal(request, "GET /", 5)){
		it += 5;
	}else if(!strict_compare_inequal(request, "HEAD /", 6)){
		request_obj->objectValues["method"]->stringValue = "HEAD";
		it += 6;
	}else if(!strict_compare_inequal(request, "POST /", 6)){
		request_obj->objectValues["method"]->stringValue = "POST";
		it += 6;
	}else if(!strict_compare_inequal(request, "DELETE /", 8)){
		request_obj->objectValues["method"]->stringValue = "DELETE";
		it += 8;
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
		case 3:
			if(*it == '+'){
				new_value += ' ';
				break;
			}
			[[clang::fallthrough]];
		case 0:
		case 1:
		case 4:
		case 6: //TODO: Other escape sequences. (e.g. '&' cannot exist in URL param value)
			if(*it == '%'){
				if(*(it + 1) == '2' && *(it + 2) == '6'){
					it++;
					it++;
					new_value += '"';
				}else if(*(it + 1) == '0' && *(it + 2) == 'A'){
					it++;
					it++;
					new_value += '\n';
				}
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
		if(request_obj->HasObj("Content-Length", STRING) &&
		std::strlen(it) != static_cast<unsigned long>(std::stol(request_obj->GetStr("Content-Length")))){
			return JSON;
		}
		
		request_obj->parse(it);
		return HTTP_API;
	}

	return HTTP;
}

std::string Util::sha256_hash(std::string data){
	std::string hash;
	CryptoPP::SHA256 hasher;

	CryptoPP::StringSource source(data, true,
		new CryptoPP::HashFilter(hasher,
		new CryptoPP::Base64Encoder(
		new CryptoPP::StringSink(hash))));

	return hash;
}

bool Util::endsWith(const std::string& str, const std::string& suffix)
{
	return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

bool Util::startsWith(const std::string& str, const std::string& prefix)
{
	return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

std::string Util::hash_value_argon2d(std::string password){
	const uint32_t t_cost = 5;
	const uint32_t m_cost = 1 << 16; //about 65MB
	const uint32_t parallelism = 1; //TODO: can use std::thread::hardware_concurrency();?

	std::vector<uint8_t> pwdvec(password.begin(), password.end());
	uint8_t* pwd = &pwdvec[0];
	const size_t pwdlen = password.length();

	//TODO: Should this be used??
	//INFO: If this changes, user passwords in DB will become invalid.
	std::string nonce = "itmyepicsalt!@12";
	std::vector<uint8_t> saltvec(nonce.begin(), nonce.end());
	uint8_t* salt = &saltvec[0];
	const size_t saltlen = nonce.length();

	uint8_t hash[128];
	
	char encoded[256];

	argon2_type type = Argon2_d;

	int res = argon2_hash(t_cost, m_cost, parallelism,
		pwd, pwdlen, salt, saltlen,
		hash, 128, encoded, 256, type, ARGON2_VERSION_NUMBER);
	
	if(res){
		ERROR("Hashing error: " << res)
		return std::string();
	}else{
		return std::string(encoded);
	}
}

void Util::print_bits(const char* data, size_t data_length){
	std::bitset<8> bits;
	for(size_t i = 0; i < data_length; ++i){
		bits = std::bitset<8>(static_cast<unsigned char>(data[i]));
		std::cout << bits << ' ';
		if(i % 4 == 3){
			std::cout << std::endl;
		}
	}
	std::cout << std::endl;
}
