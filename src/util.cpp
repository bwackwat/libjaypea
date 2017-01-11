#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <thread>
#include <ctime>
#include <tuple>
#include <vector>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "util.hpp"

std::vector<struct Argument> Util::arguments;

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

void Util::parse_arguments(int argc, char** argv, std::string description){
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
					PRINT("     --" << arg.name << " <string:" << arg.name << '>')
					for(auto& alt : arg.alts){
						PRINT("  OR " << alt << " <string:" << arg.name << '>')
					}
					PRINT("     (Default value: " << arg.string_value.get() << ")\n")
					break;
				case ARG_INTEGER:
					PRINT("     --" << arg.name << " <integer:" << arg.name << '>')
					for(auto& alt : arg.alts){
						PRINT("  OR " << alt << " <integer:" << arg.name << '>')
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

	std::ifstream config_file(CONFIG_PATH);
	if(config_file.is_open()){
		std::string config_data((std::istreambuf_iterator<char>(config_file)),
			(std::istreambuf_iterator<char>()));
		config_object.parse(config_data.c_str());
		PRINT("Loaded " << CONFIG_PATH)
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
		PRINT("Could not open " << CONFIG_PATH << ", ignoring.")
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
