#include <string>
#include <cstring>
#include <iostream>
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
#include "symmetric-event-client.hpp"

static bool stdin_available(){
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	select(STDIN_FILENO + 1, &fds, 0, 0, &tv);
	return (FD_ISSET(0, &fds));
}

[[noreturn]] static void sigcatch(int sig){
	PRINT("caught " << sig << " signal!")
	ttyreset(STDIN_FILENO);
	exit(0);
}

int main(int argc, char** argv){
	int port = 20000;
	std::string keyfile;
	std::string distribution_path = "extras/distribution-example.json";

	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("distribution_file", distribution_path, {"-df"});
	Util::parse_arguments(argc, argv, "This is a client for managing distributed servers.");

	std::ifstream distribution_file(distribution_path);
	if(!distribution_file.is_open()){
		PRINT("Could not open distribution file.")
		return 1;
	}
	
	std::string distribution_data((std::istreambuf_iterator<char>(config_file)), (std::istreambuf_iterator<char>()));
	JsonObject distribution;
	distribution.parse(distribution_data.c_str());
	PRINT(distribution.stringify(true));

	SymmetricEventClient client(keyfile);
	
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
		if(!node.HasObj("node", STRING){
			PRINT("Distribution JSON: A live-node is missing \"node\" string.")
			return 1;
		}
		if(node->HasObject("hostname", STRING)){
			client.add(new Connection(node->GetStr("hostname"), static_cast<uint16_t>(port)));
		}else if(node->HasObj("ip_address", STRING)){
			client.add(new Connection(static_cast<uint16_t>(port), node.GetStr("ip_address"));
		}else{
			PRINT("Distribution JSON: A live-node is missing both \"hostname\" and alternative \"ip_address\" strings.")
			return 1;
		}
	}
	
	for(auto iter = this->routemap.begin(); iter != this->routemap.end(); ++iter){

	if(!use_ip){
		client.add(new Connection(hostname, static_cast<uint16_t>(port)));
	}else if(!ip_address.empty()){
		client.add(new Connection(static_cast<uint16_t>(port), ip_address));
	}else{
		PRINT("You must provide either a hostname or an ip_address!")
		return 1;
	}

	std::string password;
	std::cout << "Enter the password for the distribution: ";
	std::getline(std::cin, password);

	client.on_connect = [&](int fd){
		if(client.send(fd, password)){
			ERROR("client send password")
			return true;
		}
		return false;
	};

	client.run([&](int fd, const char* data, ssize_t data_length)->ssize_t{
		switch(state){
		case VERIFY_IDENTITY:
			PRINT(data)
			if(client.send(fd, ROUTINES[routine].c_str(), ROUTINES[routine].length())){
				return -1;
			}
			state = SELECT_ROUTINE;
			break;
		case SELECT_ROUTINE:
			PRINT(data)
			switch(routine){
			case SHELL:
				std::cout << "\n\r" << hostname << " > " << std::flush;

				Util::set_non_blocking(STDIN_FILENO);

				std::signal(SIGINT, sigcatch);
				std::signal(SIGQUIT, sigcatch);
				std::signal(SIGTERM, sigcatch);

				/*
				 * Major bug note:
				 * When using "fd" within the lambda, the int "fd" inside would corrupt to an
				 * apparently random number like 32767 (near the fd limit). This is some form
				 * of a complex overflow bug, due to the fact that the "fd" originally
				 * passed into the on_read std::function within tcp-event-client is coming
				 * from a class pointer!
				 *
				 * Compare that to comd where "fd" does get directly passed into the lambda,
				 * but no issues arise because it is coming from an int member data (not a
				 * contrived class pointer). The exact reason for why a class pointer's variable
				 * caused such unsung havoc is unknown, yet likely has to do with a shifting
				 * underlyign stack of pointers, and the int is accesses by referece here...
				 */
				shell_client_fd = fd;
				client.on_event_loop = [&](){
					if(!stdin_available()){
						return false;
					}
					if((len = read(STDIN_FILENO, packet, PACKET_LIMIT)) < 0){
						if(errno != EWOULDBLOCK){
							ERROR("stdin read")
							return true;
						}
					}else if(len == 0){
						ERROR("stdin read zero");
						return true;
					}else{
						packet[len] = 0;
						if(client.send(shell_client_fd, packet, static_cast<size_t>(len))){
							return true;
						}
					}
					return false;
				};
				break;
			case SEND_FILE:
				//TODO send file name
				break;
			case RECV_FILE:
				//TODO write ok packet???
				break;
			}
			state = EXCHANGE_PACKETS;
			break;
		case EXCHANGE_PACKETS:
			std::cout << data;
			std::cout << '\r' << hostname << " > " << std::flush;
			break;
		}
		return data_length;
	});

	if(routine == SHELL){
		if(ttyreset(STDIN_FILENO) < 0){
			ERROR("ttyreset")
		}
	}

	return 0;
}
