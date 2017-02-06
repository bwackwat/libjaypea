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
		UPDATING
	};
	enum NodeState state;
	
	SymmetricTcpServer server(keyfile, port, 1);
	std::string password;
	//std::unordered_map<std::string, SymmetricTcpClient *> node_clients;
	
	server.on_connect = [&](int){
		state = VERIFYING;
	};

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		if(state == VERIFYING){
			if(password.empty()){
				password = data;
				if(server.send(fd, "Please update the distribution file.")){
					PRINT("send error 1")
					return -1;
				}
			}else if(!Util::strict_compare_inequal(data, password.c_str(), password.length())){
				return -1;
			}
			if(server.send(fd, distribution_string){
				PRINT("send error")
				return -1;
			}
			state = COLLECTING;
			return data_length;
		}else if(state == COLLECTING){
				
