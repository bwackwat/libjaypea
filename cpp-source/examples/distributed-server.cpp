#include "util.hpp"
#include "json.hpp"
#include "distributed-util.hpp"
#include "symmetric-tcp-server.hpp"

int main(int argc, char** argv){
	int port = 10001;
	std::string keyfile;
	
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::parse_arguments(argc, argv, "This server manages others of its kind.");
	
	SymmetricEpollServer server(keyfile, static_cast<uint16_t>(port), 1);
	
	enum ServerState{
		VERIFYING,
		EXCHANGING
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
			state = EXCHANGING;
			return data_length;
		}else{
			if(data == GET){
				if(server.send(fd, "GET")){
					PRINT("send error 2")
					return -1;
				}
			}else if(data == SET){
				if(server.send(fd, "SET")){
					PRINT("send error 2")
					return -1;
				}
			}else if(data == SHELL){
				if(server.send(fd, "Shell entered.")){
					PRINT("send error 2")
					return -1;
				}
			}else if(data == EXIT){
				if(server.send(fd, "Shell is done.")){
					PRINT("send error 2")
					return -1;
				}
			}else{
				if(server.send(fd, "Shell is done.")){
					PRINT("send error 2")
					return -1;
				}
			}
		}
		return data_length;
	};
	
	server.run(false, 1);
	
	// This should only return on reboot, necessary for written configuration to activate.
	
	return 0;
}
