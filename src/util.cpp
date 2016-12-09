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

std::map<std::string, std::reference_wrapper<std::string>> Util::string_arguments;
std::map<std::string, int*> Util::int_arguments;
std::map<std::string, bool*> Util::bool_arguments;

std::map<std::string, std::function<void(char*)>> Util::argument_resolvers;
std::map<std::string, std::vector<std::string>> Util::argument_flags;
std::vector<std::string> Util::required_arguments;

std::vector<struct Argument> Util::arguments;

void Util::define_argument(std::string name, std::string& value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	arguments.push_back({name, alts, callback, required, ARG_STRING, std::ref(value), 0, 0});
}

void Util::define_argument(std::string name, int* value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	arguments.push_back({name, alts, callback, required, ARG_INTEGER, std::ref(name), value, 0});
}

void Util::define_argument(std::string name, bool* value, std::vector<std::string> alts, std::function<void()> callback, bool required){
	arguments.push_back({name, alts, callback, required, ARG_BOOLEAN, std::ref(name), 0, value});
}

/*
void Util::define_argument(std::string name, void* value, std::function<void(char*)> resolver, enum ArgumentType type, std::vector<std::string> alts, bool required){
	arguments.push_back({name, value, resolver, type, alts, required});
}

void Util::define_argument(std::string name, std::vector<std::string> alts, std::function<void(char*)> resolver, bool required){
	argument_resolvers[name] = resolver;
	argument_flags[name] = alts;
	if(required){
		Util::required_arguments.push_back(name);
	}
}

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
*/
void Util::parse_arguments(int argc, char** argv, std::string description){
	std::string check;
	std::cout << "Arguments: ";
	for(int i = 0; i < argc; ++i){
		std::cout << argv[i] << ' ';
	}
	std::cout << '\n';
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
		for(auto& arg: arguments){
			check = "--" + arg.name;
			std::function<bool(const char*)> check_lambda;
			switch(arg.type){
			case ARG_STRING:
				check_lambda = [argc, i, arg, argv](const char* check_sub) mutable -> bool{
					if(std::strcmp(argv[i], check_sub) == 0){
						if(argc > i){
							arg.string_value.get() = std::string(argv[i + 1]);
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
						if(argc > i){
							*arg.integer_value = std::stoi(argv[i + 1]);
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
}
/*
void Util::parse_arguments(int argc, char** argv, std::string description){
	std::string check;
	std::cout << "Arguments: ";
	for(int i = 0; i < argc; ++i){
		std::cout << argv[i] << ' ';
	}
	std::cout << '\n';
	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "--help") == 0 ||
		std::strcmp(argv[i], "-h") == 0){
			PRINT("Usage: " << argv[0])
			PRINT("     --help")
			PRINT("  OR -h\n")
			for(auto iter = string_arguments.begin(); iter != string_arguments.end(); ++iter){
				if(std::find(required_arguments.begin(), required_arguments.end(), iter->first) != required_arguments.end()){
					PRINT("     REQUIRED")
				}
				PRINT("     --" << iter->first << " <string:" << iter->first << '>')
				for(auto& flag : argument_flags[iter->first]){
					PRINT("  OR " << flag << " <string:" << iter->first << '>')
				}
				PRINT("     (Default value: " << iter->second.get() << ")\n")
			}
			for(auto iter = int_arguments.begin(); iter != int_arguments.end(); ++iter){
				if(std::find(required_arguments.begin(), required_arguments.end(), iter->first) != required_arguments.end()){
					PRINT("     REQUIRED")
				}
				PRINT("     --" << iter->first << " <integer:" << iter->first << '>')
				for(auto& flag : argument_flags[iter->first]){
					PRINT("  OR " << flag << " <integer:" << iter->first << '>')
				}
				PRINT("     (Default value: " << *iter->second << ")\n")
			}
			for(auto iter = bool_arguments.begin(); iter != bool_arguments.end(); ++iter){
				PRINT("     --" << iter->first)
				for(auto& flag : argument_flags[iter->first]){
					PRINT("  OR " << flag)
				}
				PRINT("     (Disabled by default.)\n")
			}
			PRINT(description)
			exit(0);
		}
		bool got_argument = false;
		for(auto iter = string_arguments.begin(); iter != string_arguments.end(); ++iter){
			check = "--" + iter->first;
			auto check_lambda = [argc, i, iter, argv, check, got_argument]() mutable{
				if(argc > i){
					iter->second.get() = std::string(argv[i + 1]);
					got_argument = true;
				}else{
					PRINT("No string provided for " << check)
				}
			};
			if(std::strcmp(argv[i], check.c_str()) == 0){
				check_lambda();
				break;
			}
			for(auto& flag : argument_flags[iter->first]){
				if(std::strcmp(argv[i], flag.c_str()) == 0){
					check_lambda();
					break;
				}
			}
		}
		if(got_argument)continue;
		for(auto iter = int_arguments.begin(); iter != int_arguments.end(); ++iter){
			check = "--" + iter->first;
			auto check_lambda = [argc, i, iter, argv, check, got_argument]() mutable{
				if(argc > i){
					*iter->second = std::stoi(argv[i + 1]);
					got_argument = true;
				}else{
					PRINT("No integer provided for " << check)
				}
			};
			if(std::strcmp(argv[i], check.c_str()) == 0){
				check_lambda();
				break;
			}
			for(auto& flag : argument_flags[iter->first]){
				if(std::strcmp(argv[i], flag.c_str()) == 0){
					check_lambda();
					break;
				}
			}
		}
		if(got_argument)continue;
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
}*/

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


