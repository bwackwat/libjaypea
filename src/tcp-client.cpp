#include <thread>
#include <vector>
#include <cstring>

#include "util.hpp"
#include "simple-tcp-client.hpp"

static int requests = 1;
static bool verbose = false;

static const char* request = "GET / HTTP/1.1\n"
"User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:45.0) Gecko/20100101 Firefox/45.0\n"
"Accept: image/png,image/*;q=0.8,*/*;q=0.5\n"
"Accept-Language: en-US,en;q=0.5\n"
"Accept-Encoding: gzip, deflate\n"
"Connection: keep-alive\n\n";
static size_t request_length = strlen(request);

static void handle_connection(SimpleTcpClient* client, int connection){
	char response[PACKET_LIMIT];
	for(int i = 0; i < requests; ++i){
		try{
			if(!client->communicate(request, request_length, response)){
				PRINT("Server kicked client!");
				break;
			}
			if(verbose){
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
	std::string hostname = "localhost";
	int port = 80, connections = 1;
	struct hostent* host;

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("connections", &connections, {"-c"});
	Util::define_argument("requests", &requests, {"-r"});
	Util::define_argument("verbose", &verbose, {"-v"});
	Util::parse_arguments(argc, argv, "This program will make threads for connections, each making a bunch of requests.");

	if((host = gethostbyname(hostname.c_str())) == 0){
		ERROR("gethostbyname")
		return 1;
	}
	struct in_addr host_addr = *reinterpret_cast<struct in_addr*>(host->h_addr);

	std::vector<std::thread> thread_list;
	for(int i = 1; i <= connections; ++i){
		thread_list.push_back(std::thread(handle_connection, new SimpleTcpClient(static_cast<uint16_t>(port), host_addr), i));
	}

	for(auto &t : thread_list){
		t.join();
	}

	PRINT("DONE!")

	return 0;
}
