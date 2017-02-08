#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <csignal>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.hpp"
#include "comd-util.hpp"
#include "symmetric-tcp-client.hpp"

int main(int argc, char** argv){
	int port = 20000;
	std::string keyfile;
	std::string distribution_path = "extras/distribution-example.json";

	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("distribution_file", distribution_path, {"-df"});
	Util::parse_arguments(argc, argv, "This is a client for managing distributed servers.");

	std::ifstream distribution_file(distribution_path);
	if(!distribution_file.is_open()){
		PRINT("Could not open distribution file.")
		return 1;
	}
	std::string distribution_data((std::istreambuf_iterator<char>(distribution_file)), (std::istreambuf_iterator<char>()));
	JsonObject distribution;
	distribution.parse(distribution_data.c_str());
	PRINT("LOADED DISTRIBUTION DEFINITIONS FILE:")
	PRINT(distribution.stringify(true));

	std::unordered_map<std::string, SymmetricTcpClient*> clients;
	
	if(!distribution.HasObj("service-definitions", ARRAY)){
		PRINT("Distribution JSON: Missing \"service-definitions\" array.")
		return 1;
	}
	if(!distribution.HasObj("node-definitions", ARRAY)){
		PRINT("Distribution JSON: Missing \"node-definitions\" object.")
		return 1;
	}
	if(!distribution.HasObj("live-nodes", ARRAY)){
		PRINT("Distribution JSON: Missing \"live-nodes\" object.")
		return 1;
	}
	for(auto node : distribution.objectValues["live-nodes"]->arrayValues){
		if(!node->HasObj("node", STRING)){
			PRINT("Distribution JSON: A live-node is missing \"node\" string.")
			return 1;
		}
		
		if(node->HasObj("hostname", STRING)){
			clients[node->GetStr("hostname")] = new SymmetricTcpClient(node->GetStr("hostname"), static_cast<uint16_t>(port), keyfile);
		}else if(node->HasObj("ip_address", STRING)){
			clients[node->GetStr("ip_address")] = new SymmetricTcpClient(node->GetStr("ip_address").c_str(), static_cast<uint16_t>(port), keyfile);
		}else{
			PRINT("Distribution JSON: A live-node is missing both \"hostname\" and alternative \"ip_address\" strings.")
			return 1;
		}
	}

	PRINT("CAREFUL! YOU'RE USING A SENSITIVE TOOL.")

	std::string password;
	std::cout << "Enter the password for the distribution: ";
	std::getline(std::cin, password);

	std::string commands = "Commands:\n"
		"\thelp\n"
		"\tPrints this.\n\n"
		"\tservers\n"
		"\tLists the servers (from " + distribution_path + ".)\n\n"
		"\tuse <hostname or ip_address>\n"
		"\tSend commands to a SINGLE server.\n\n"
		"\tbroadcast\n"
		"\tSend commands to ALL servers.\n\n"
		"\tget\n"
		"\tGet the configuration.\n\n"
		"\tset\n"
		"\tWill set the server configuration(s) based on the loaded distribution file.\n"
		"\tThe command will first reveal the changes to be made and ask for a confirmation.\n\n"
		"\tshell\n"
		"\tWill start the remote shell(s), and commands will be sent to it.\n\n"
		"\texit\n"
		"\tWill exit the remote shell(s), or will exit the client entirely.\n\n";
	std::string HELP = "help";
	std::string SERVERS = "servers";
	std::string USE = "use ";
	std::string BROADCAST = "broadcast";
	std::string GET = "get";
	std::string SET = "set";
	std::string SHELL = "shell";
	std::string EXIT = "exit";

	std::string selected_client;
	std::string request;

	std::function<void(const std::string&, std::string&)> send =
	[&](const std::string& client, std::string& message){
		std::string response = clients[client]->communicate(message);
		if(response.empty()){
			PRINT(client << ": Disconnected!")
		}else{
			PRINT(client << ": " << response)
		}
	};

	std::function<void(std::string&)> broadcast = [&](std::string& message){
		for(auto iter = clients.begin(); iter != clients.end(); ++iter){
			if(!iter->second->connected){
				send(iter->first, password);
			}
			send(iter->first, message);
		}
	};

	PRINT(commands)

	while(true){
		if(selected_client.empty()){
			std::cout << "Broadcast: ";
		}else{
			std::cout << selected_client << " :";
		}

		std::getline(std::cin, request);

		if(request == HELP){
			PRINT(commands)
		}else if(request == SERVERS){
			for(auto iter = clients.begin(); iter != clients.end(); ++iter){
				std::cout << iter->first;
				if(iter->second->connected){
					PRINT(": Connected")
				}else{
					PRINT(": Disconnected")
				}
			}
		}else if(request.length() > 4 && request.substr(0, 4) == USE){
			if(clients.count(request.substr(4))){
				selected_client = request.substr(4);
			}else{
				PRINT("Invalid hostname or ip_address.")
			}
		}else if(request == BROADCAST){
			selected_client = std::string();
		}else if(request == GET){
			if(selected_client.empty()){
				broadcast(GET);
			}else{
				send(selected_client, GET);
			}
		}else if(request == SET){
			PRINT("ARE YOU SURE LOL?")
		}else if(request == SHELL){
			PRINT("SHELL STARTED ON")
		}else if(request == EXIT){
			break;
		}	
	}

	PRINT("DONE!")

	return 0;
}
