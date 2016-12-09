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

/*void Util::initialize_arguments(int argc, char** argv, std::vector<std::tuple<enum ArgumentType, std::string, void*, bool>> arguments, std::string description){
	PRINT("Arguments:")
	for(int i = 0; i < argc ++i){
		PRINT(argv[i])
	}	
	for(int i = 0; i < argc ++i){
		if(std::strcmp(argv[i], "--help") == 0 ||
		std::strcmp(argv[i], "-h") == 0){
			PRINT("Usage: ")
			for(auto& argument : arguments){
				if(std::get<0>(argument) == ARG_BOOLEAN){
					PRINT("\t--" << std::get<1>(argument))
				}else{
					PRINT("\t--" << std::get<1>(argument) << " <" << std::get<1>(argument) << '>')
				}
			}
			PRINT("\n\n" << description)
			exit(0);
		}
		for(auto& argument : arguments){
			std::string arg_string = "--" + std::get<1>(argument);
			if(std::strcmp(argv[i], arg_string.c_str()) == 0){
				switch(std::get<0>(argument)){
				case ARG_STRING:
					if(argc > i){
						*std::get<2>(argument) = std::string(argv[i + 1]);
					}else{
						PRINT("No value for " << std::get<1>(argument))
					}
					break;
				case ARG_INTEGER:
					if(argc > i){
						*std::get<2>(argument) = std::stoi(argv[i + 1]);
					}else{
						PRINT("No value for " << std::get<1>(argument))
					}
					break;
				case ARG_BOOLEAN:
					*std::get<2>(argument) = true;
					break;
				}
				break;
			}
		}
	}
}*/

std::map<std::string, std::reference_wrapper<std::string>> Util::string_arguments;
std::map<std::string, int*> Util::int_arguments;
std::map<std::string, bool*> Util::bool_arguments;
std::map<std::string, std::vector<std::string>> Util::argument_flags;
std::vector<std::string> Util::required_arguments;

void Util::define_argument(std::string name, std::string& value, std::vector<std::string> flags, bool required){
	string_arguments.emplace(name, std::ref(value));
	argument_flags[name] = flags;
	if(required){
		required_arguments.push_back(name);
	}
}

void Util::define_argument(std::string name, int* value, std::vector<std::string> flags, bool required){
	int_arguments[name] = value;
	argument_flags[name] = flags;
	if(required){
		Util::required_arguments.push_back(name);
	}
}

void Util::define_argument(std::string name, bool* value, std::vector<std::string> flags, bool required){
	Util::bool_arguments[name] = value;
	Util::argument_flags[name] = flags;
	if(required){
		Util::required_arguments.push_back(name);
	}
}

void Util::parse_arguments(int argc, char** argv, std::string description){
	bool got_argument;
	std::string check;

	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "--help") == 0 ||
		std::strcmp(argv[i], "-h") == 0){
			PRINT("Usage:")
			for(auto iter = string_arguments.begin(); iter != string_arguments.end(); ++iter){
				std::cout << '\t';
				if(std::find(required_arguments.begin(), required_arguments.end(), iter->first) != required_arguments.end()){
					std::cout << "(REQUIRED) ";
				}
				PRINT("--" << iter->first << " <string:" << iter->first << '>')
				for(auto& flag : argument_flags[iter->first]){
					PRINT("\t OR -" << flag << " <string:" << iter->first << '>')
				}
				PRINT("\t(Default value: " << iter->second.get() << "\n")
			}
			for(auto iter = int_arguments.begin(); iter != int_arguments.end(); ++iter){
				std::cout << '\t';
				if(std::find(required_arguments.begin(), required_arguments.end(), iter->first) != required_arguments.end()){
					std::cout << "(REQUIRED) ";
				}
				PRINT("--" << iter->first << " <integer:" << iter->first << '>')
				for(auto& flag : argument_flags[iter->first]){
					PRINT("\t OR -" << flag << " <integer:" << iter->first << '>')
				}
				PRINT("\t(Default value: " << *iter->second << ")\n")
			}
			for(auto iter = bool_arguments.begin(); iter != bool_arguments.end(); ++iter){
				PRINT("\t(boolean toggle) --" << iter->first)
				for(auto& flag : argument_flags[iter->first]){
					PRINT("\t OR " << flag)
				}
				PRINT("\t(Disabled by default.)\n")
			}
			PRINT(description)
			exit(0);
		}
		got_argument = false;
		for(auto iter = string_arguments.begin(); iter != string_arguments.end(); ++iter){
			check = "--" + iter->first;
			if(std::strcmp(argv[i], check.c_str()) == 0){
				if(argc > i){
					iter->second.get() = std::string(argv[i + 1]);
					got_argument = true;
				}else{
					PRINT("No string provided for " << check)
				}
				break;
			}
			for(auto& flag : argument_flags[iter->first]){
				if(std::strcmp(argv[i], flag.c_str()) == 0){
					if(argc > i){
						iter->second.get() = std::string(argv[i + 1]);
						got_argument = true;
					}else{
						PRINT("No string provided for " << flag)
					}
					break;
				}
			}
		}
		if(got_argument){
			continue;
		}
		for(auto iter = int_arguments.begin(); iter != int_arguments.end(); ++iter){
			check = "--" + iter->first;
			if(std::strcmp(argv[i], check.c_str()) == 0){
				if(argc > i){
					*iter->second = std::stoi(argv[i + 1]);
					got_argument = true;
				}else{
					PRINT("No integer provided for " << check)
				}
				break;
			}
			for(auto& flag : argument_flags[iter->first]){
				if(std::strcmp(argv[i], flag.c_str()) == 0){
					if(argc > i){
						*iter->second = std::stoi(argv[i + 1]);
						got_argument = true;
					}else{
						PRINT("No integer provided for " << flag)
					}
					break;
				}
			}
		}
		if(got_argument){
			continue;
		}
		for(auto iter = bool_arguments.begin(); iter != bool_arguments.end(); ++iter){
			check = "--" + iter->first;
			if(std::strcmp(argv[i], check.c_str()) == 0){
				*iter->second = true;
				break;
			}
			for(auto& flag : argument_flags[iter->first]){
				if(std::strcmp(argv[i], flag.c_str()) == 0){
					*iter->second = true;
					break;
				}
			}
		}
	}
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


