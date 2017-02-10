#include <fstream>

#include "util.hpp"
#include "distributed-util.hpp"
#include "symmetric-tcp-client.hpp"

int main(int argc, char** argv){
	int port = 10001;
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
	PRINT("LOADED DISTRIBUTION FILE: " + distribution_path)
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

	// Maps service name to service object.
	std::unordered_map<std::string, JsonObject*> services;

	for(auto service : distribution.objectValues["service-definitions"]->arrayValues){
		if(!service->HasObj("name", STRING)){
			PRINT("A service-definition is missing \"name\" string.")
			return 1;
		}
		services[service->GetStr("name")] = service;
	}
	
	// Maps node names to services arrays.
	std::unordered_map<std::string, JsonObject*> nodes;

	for(auto node : distribution.objectValues["node-definitions"]->arrayValues){
		if(!node->HasObj("name", STRING)){
			PRINT("A node-definition is missing \"name\" string.")
			return 1;
		}
		if(!node->HasObj("services", ARRAY)){
			PRINT("A node-definition is missing \"services\" array.")
			return 1;
		}
		nodes[node->GetStr("name")] = node;
	}

	std::function<void(const std::string&, std::string&)> send_to =
	[&](const std::string& client, std::string& message){
		std::string response = clients[client]->communicate(message);
		if(response.empty()){
			PRINT(client << ": Disconnected!")
		}else{
			PRINT(client << ": " << response)
		}
	};

	std::string password;
	std::cout << "Enter the password for the distribution: ";
	std::getline(std::cin, password);

	std::unordered_map<std::string, JsonObject*> host_services;

	std::string name;
	for(auto node : distribution.objectValues["live-nodes"]->arrayValues){
		if(!node->HasObj("node", STRING)){
			PRINT("Distribution JSON: A live-node is missing \"node\" string.")
			return 1;
		}
	
		if(node->HasObj("hostname", STRING)){
			name = node->GetStr("hostname");
			clients[name] = new SymmetricTcpClient(name, static_cast<uint16_t>(port), keyfile);
		}else if(node->HasObj("ip_address", STRING)){
			name = node->GetStr("ip_address");
			clients[name] = new SymmetricTcpClient(name.c_str(), static_cast<uint16_t>(port), keyfile);
		}else{
			PRINT("Distribution JSON: A live-node is missing both \"hostname\" and alternative \"ip_address\" strings.")
			return 1;
		}
		send_to(name, password);
		host_services[name] = new JsonObject(ARRAY);
		for(auto service : nodes[node->GetStr("node")]->objectValues["services"]->arrayValues){
			host_services[name]->arrayValues.push_back(services[service->stringValue]);
		}
	}

	std::string commands = "Commands:\n"
		"\thelp\n"
		"\tPrints this.\n\n"
		"\tservers\n"
		"\tLists the servers (from \"" + distribution_path + "\".)\n\n"
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
		"\tWill exit the remote shell(s), or will exit the client entirely.\n";

	std::string selected_client;
	std::string request;
	bool in_shell = false;

	std::function<void(std::string&)> broadcast = [&](std::string& message){
		for(auto iter = clients.begin(); iter != clients.end(); ++iter){
			if(!iter->second->connected){
				send_to(iter->first, password);
			}
			if(iter->second->connected){
				send_to(iter->first, message);
			}
		}
	};
	
	std::function<void(std::string&)> send = [&](std::string& message){
		if(selected_client.empty()){
			broadcast(message);
		}else{
			send_to(selected_client, message);
		}
	};

	// If not ignored, write() on a closed TCP connection will signal instead of error.
	std::signal(SIGPIPE, SIG_IGN);

	PRINT(commands)

	while(true){
		if(selected_client.empty()){
			std::cout << "Broadcast";
		}else{
			std::cout << selected_client;
		}
		if(in_shell){
			std::cout << " /bin/bash >>> ";
		}else{
			std::cout << " >>> ";
		}

		std::getline(std::cin, request);
		
		if(in_shell){
			if(request == EXIT){
				in_shell = false;
			}
			send(request);
		}else if(request == HELP){
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
			send(GET);
		}else if(request == SET){
			send(SET);
			for(auto iter = clients.begin(); iter != clients.end(); ++iter){
				if(!iter->second->connected){
					PRINT(iter->first << ": Disconnected")
					continue;
				}
				// TODO: Rethink this. It sets hostname unqiuely.
				for(auto service : host_services[iter->first]->arrayValues){
					if(service->HasObj("hostname", STRING)){
						service->objectValues["hostname"]->stringValue = iter->first;
					}
				}
				std::string temp_services = host_services[iter->first]->stringify(true) + "\n";
				send_to(iter->first, temp_services);
			}
		}else if(request == SHELL){
			send(SHELL);
			in_shell = true;
		}else if(request == EXIT){
			break;
		}else{
			PRINT("Unknown command: \"" << request << "\". Try \"help\".")
		}
	}

	PRINT("DONE!")

	return 0;
}
