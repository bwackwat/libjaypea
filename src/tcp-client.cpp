#include <thread>
#include <vector>
#include <cstring>

#include "util.hpp"
#include "simple-tcp-client.hpp"

int requests = 1;
int verbosity = 0;

const char* request = "GET /api HTTP/1.1\n"
"Host: 10.174.13.94\n"
"User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:45.0) Gecko/20100101 Firefox/45.0\n"
"Accept: image/png,image/*;q=0.8,*/*;q=0.5\n"
"Accept-Language: en-US,en;q=0.5\n"
"Accept-Encoding: gzip, deflate\n"
"Referer: https://10.174.13.94/\n"
"Cookie: session=d69ad642-92d1-4147-a3d5-6d8896731570\n"
"Connection: keep-alive\n\n";
size_t request_length = strlen(request);

void handle_connection(SimpleTcpClient* client, int connection){
	char response[PACKET_LIMIT];
	for(int i = 0; i < requests; ++i){
		try{
			client->communicate(request, request_length, response);
			if(verbosity){
				std::cout << "Response: " << response << std::endl;
			}
		}catch(std::exception& e){
			std::cout << e.what() << std::endl;
		}
	}
	std::cout << "Connection #" << connection << " done." << std::endl;
	delete client;
}

int main(int argc, char** argv){
	struct hostent* host;
	std::string hostname = "localhost";
	uint16_t port = 80;
	int connections = 1;

	for(int i = 0; i < argc; i++){
		if((std::strcmp(argv[i], "-v") == 0 ||
		std::strcmp(argv[i], "--verbose") == 0) && argc > i){
			verbosity = 1;
		}else if((std::strcmp(argv[i], "-c") == 0 ||
		std::strcmp(argv[i], "--connections") == 0) && argc > i){
			connections = std::stoi(argv[i + 1]);
		}else if((std::strcmp(argv[i], "-r") == 0 ||
		std::strcmp(argv[i], "--requests") == 0) && argc > i){
			requests = std::stoi(argv[i + 1]);
		}else if((std::strcmp(argv[i], "-p") == 0 ||
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

	if((host = gethostbyname(hostname.c_str())) == 0){
		ERROR("gethostbyname")
		return 1;
	}
	struct in_addr host_addr = *reinterpret_cast<struct in_addr*>(host->h_addr);
	
	std::vector<std::thread> thread_list;

	for(int i = 1; i <= connections; ++i){
		thread_list.push_back(std::thread(handle_connection, new SimpleTcpClient(port, host_addr), i));
	}

	for(auto &t : thread_list){
		t.join();
	}

	PRINT("DONE!")

	return 0;
}
