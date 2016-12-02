#include "util.hpp"

int main(int argc, char** argv){
	int serverfd, clientfd, num_clients = 1, num = 1;
	long rlen = 1, wlen;
	struct sockaddr_in server_addr;
	struct hostent* host;
	uint addr_len = sizeof(client_addr);
	std::string hostname = "localhost";

	char packet[PACKET_LIMIT];

	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "-h") == 0){
			PRINT("https://github.com/bwackwat/event-spiral")
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

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr = *reinterpret_cast<struct in_addr*>(host->h_addr);
	server_addr.sin_port = htons(PORT);
	bzero(&(server_addr.sin_zero), 8);

	return 0;
}
