#include <fstream>

#include "util.hpp"
#include "json.hpp"
#include "shell.hpp"
#include "distributed-util.hpp"
#include "symmetric-epoll-server.hpp"

int main(int argc, char** argv){
	int port;
	std::string keyfile;
	std::string services_path = "artifacts/services.json";
	
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("services_file", services_path, {"-sf"});
	Util::parse_arguments(argc, argv, "This server manages services on its host.");
	
	std::ifstream services_file(services_path);
	JsonObject services;
	if(services_file.is_open()){
		std::string services_data((std::istreambuf_iterator<char>(services_file)), (std::istreambuf_iterator<char>()));
		services.parse(services_data.c_str());
		PRINT("LOADED SERVICES FILE: " << services_path)
		PRINT(services.stringify(true));
	}

	SymmetricEpollServer server(keyfile, static_cast<uint16_t>(port), 1);

	Shell shell;
	
	enum ServerState{
		VERIFYING,
		GET_ROUTINE,
		SET_SERVICES,
		IN_SHELL
	} state;
	
	std::string password;
	
	server.on_connect = [&](int){
		state = VERIFYING;
	};

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT("RECV:" << data)
		if(state == VERIFYING){
			if(password.empty()){
				password = std::string(data);
				if(server.send(fd, "Password has been set, please set the configuration.")){
					PRINT("send error 1")
					return -1;
				}
			}else if(Util::strict_compare_inequal(data, password.c_str(), static_cast<int>(password.length()))){
				PRINT("Someone failed to verify their connection to this server!")
				return -1;
			}else if(server.send(fd, "Password accepted.")){
				PRINT("send error 2")
				return -1;
			}
			state = GET_ROUTINE;
			return data_length;
		}else if(state == GET_ROUTINE){
			if(data == GET){
				if(services.type == NOTYPE){
					if(server.send(fd, "No services JSON!")){
						PRINT("send error 2")
						return -1;
					}
				}else{
					if(server.send(fd, services.stringify(false))){
						PRINT("send error 2")
						return -1;
					}
				}
			}else if(data == SET){
				if(server.send(fd, "Setting services.")){
					PRINT("send error 3")
					return -1;
				}
				state = SET_SERVICES;
			}else if(data == SHELL){
				if(server.send(fd, "Shell entered.")){
					PRINT("send error 4")
					return -1;
				}
				if(shell.sopen()){
					ERROR("shell open")
					return -1;
				}
				state = IN_SHELL;
			}else{
				PRINT("unknown command" << data)
				return -1;
			}
		}else if(state == SET_SERVICES){
			services.parse(data);
			if(server.send(fd, services.stringify(true))){
				PRINT("send error 2")
				return -1;
			}

			std::stringstream setup_firewall_bash;
			setup_firewall_bash << "#!/bin/bash\n\n";
			std::stringstream start_services_bash;
			start_services_bash << "#!/bin/bash\n\n";

			for(auto service : services.arrayValues){
				std::string service_configuration_path = Util::libjaypea_path + "artifacts/" + service->GetStr("name") + ".configuration.json";
				Util::write_file(service_configuration_path, service->stringify(true));
				if(service->HasObj("port", STRING)){
					if(service->HasObj("forward-port-to", STRING)){
						setup_firewall_bash << "firewall-cmd --permanent --zone=public --add-forward-port=port=" << service->GetStr("forward-port-to") << ":proto=tcp:toport=" << service->GetStr("port") << "\n";
					}else{
						setup_firewall_bash << "firewall-cmd --permanent --zone=public --add-port=" << service->GetStr("port") << "/tcp\n";
					}
				}
				// Build service
				start_services_bash << Util::libjaypea_path << "scripts/build-example.sh " << service->GetStr("name") << " PROD > " << Util::libjaypea_path << "logs/build-" << service->GetStr("name") << ".log 2>&1\n\n";
				// Run service.
				start_services_bash << Util::libjaypea_path << "binaries/" << service->GetStr("name") << " --configuration_file " << service_configuration_path << " > " << Util::libjaypea_path << "logs/" << service->GetStr("name") << ".log 2>&1 &\n\n";
			}
			setup_firewall_bash << "\nfirewall-cmd --reload\n";
			Util::write_file(Util::libjaypea_path + "artifacts/setup-firewall.sh", setup_firewall_bash.str());
			Util::write_file(Util::libjaypea_path + "artifacts/start-services.sh", start_services_bash.str());
			Util::write_file(Util::libjaypea_path + services_path, services.stringify(true));
			state = GET_ROUTINE;
		}else if(state == IN_SHELL){
			if(data == EXIT){
				shell.sclose();
				if(server.send(fd, "Shell is done.")){
					PRINT("send error 2")
					return -1;
				}
				state = GET_ROUTINE;
			}else{
				ssize_t len;
				char shell_data[PACKET_LIMIT];
				if((len = write(shell.input, data, static_cast<size_t>(data_length))) < 0){
					ERROR("write to shell input")
					shell.sclose();
					return -1;
				}else if(len != data_length){
					ERROR("couldn't write all " << len << ", " << data_length)
				}

				// Just write newline for bash.
				shell_data[0] = '\n';
				shell_data[1] = 0;
				if((len = write(shell.input, shell_data, 1)) < 0){
					ERROR("write n to shell input")
					shell.sclose();
					return -1;
				}else if(len != 1){
					ERROR("couldn't write n " << len << ", " << data_length)
				}

				// Wait a second to get a bit of output if any
				sleep(1);
				if((len = read(shell.output, shell_data, PACKET_LIMIT)) < 0){
					ERROR("read from shell output")
					shell.sclose();
					return -1;
				}
				shell_data[len] = 0;
				PRINT("SHELL:" <<  shell_data)
				if(server.send(fd, shell_data, static_cast<size_t>(len))){
					PRINT("send error 5")
					shell.sclose();
					return -1;
				}
			}
		}else{
			return -1;
		}
		return data_length;
	};
	
	server.run(false, 1);
	
	// This should only return on reboot, necessary for written configuration to activate.
	
	return 0;
}
