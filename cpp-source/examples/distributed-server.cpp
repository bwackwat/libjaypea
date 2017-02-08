#include "symmetric-tcp-server.hpp"
#include "symmetric-tcp-client.hpp"
#include "json.hpp"

int main(int argc, char** argv){
	int port = 10001;
	std::string keyfile;
	std::string distribution;
	
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("distribution", distribution, {"-d"});
	Util::parse_arguments(argc, argv, "This server manages others of its kind.")

	std::ifstream distribution_file(distribution);
	if(!distribution_file.is_open()){
		PRINT("Missing distribution file.")
		return 1;
	}
	
	std::string distribution_data((std::istreambuf_iterator<char>(config_file)), (std::istreambuf_iterator<char>()));
	JsonObject distribution_object;
	distribution_object.parse(distribution_data.c_str());
	std::string distribution_string = distribution_object.stringify();
	
	PRINT(distribution_object.stringify(false));
	
	enum NodeState{
		VERIFYING,
		GET_ROUTINE,
		HEALTH_CHECK,
		SHELL,
		RECV_FILE,
		SEND_FILE,
		GET_CONFIG,
		UPDATE_CONFIG
	};
	enum NodeState state;
	
	SymmetricTcpServer server(keyfile, port, 1);
	std::string password;
	
	server.on_connect = [&](int){
		state = VERIFYING;
	};

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		if(state == VERIFYING){
			if(password.empty()){
				password = data;
				if(configuration_string.empty()){
					if(server.send(fd, "Password has been set, please update the configuration.")){
						PRINT("send error 1")
						return -1;
					}
					state == INITIALIZE_CONFIG;
					return data_length;
				}
			}else if(!Util::strict_compare_inequal(data, password.c_str(), password.length())){
				PRINT("Someone failed to verify their connection to this server!!!"
				return -1;
			}
			if(configuration_string.empty()){
				if(server.send(fd, "Configuration has not been set!"){
					PRINT("send error")
					return -1;
				}
				state = GET_ROUTINE;
				return data_length;
			}else{
				if(server.send(fd, distribution_object.stringify(true))){
					PRINT("send error")
					return -1;
				}
				state = GET_ROUTINE;
				return data_length;
			}
		}else if(state == GET_ROUTINE){
			if(!Util::strict_compare_inequal(data, health_check.c_str(), health_check.length())){
				if(
