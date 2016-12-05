#include "util.hpp"

int main(int argc, char** argv){
	int serverfd, clientfd, num_clients = 1, num = 1;
	long rlen = 1, wlen;
	struct sockaddr_in client_addr;
	struct hostent* host;
	uint addr_len = sizeof(client_addr);
	std::string hostname = "localhost";
	uint16_t port = 80;

	char packet[PACKET_LIMIT];

	for(int i = 0; i < argc; i++){
		if((std::strcmp(argv[i], "-p") == 0 ||
		std::strcmp(argv[i], "--port") == 0) && argc > i){
			port = static_cast<uint16_t>(std::stoi(argv[i + 1]));
		}else if((std::strcmp(argv[i], "-h") == 0 ||
		std::strcmp(argv[i], "--hostname") == 0) && argc > i){
			hostname = argv[i + 1];
		}else if((std::strcmp(argv[i], "-?") == 0 ||
		std::strcmp(argv[i], "--help") == 0) && argc > i){
			std::cout << "Usage: tcp_client --port <port> --hostname <hostname> --keyfile <keyfile>\n\n";
			return 0;
		}
	}

	if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		ERROR("socket")
		return 1;
	}

	if((host = gethostbyname(hostname.c_str())) == 0){
		ERROR("gethostbyname")
		return 1;
	}

	client_addr.sin_family = AF_INET;
	client_addr.sin_addr = *reinterpret_cast<struct in_addr*>(host->h_addr);
	client_addr.sin_port = htons(port);
	bzero(&(client_addr.sin_zero), 8);

	return 0;
}
