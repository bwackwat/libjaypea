#include <thread>
#include <vector>
#include <cstring>

#include "util.hpp"
#include "tcp-event-client.hpp"

int main(int argc, char** argv){
	std::string hostname = "localhost";
	int port = 80, connections = 1, requests = 1;
	bool verbose = false;

	const char* request = "GET / HTTP/1.1\n"
		"User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:45.0) Gecko/20100101 Firefox/45.0\n"
		"Accept: image/png,image/*;q=0.8,*/*;q=0.5\n"
		"Accept-Language: en-US,en;q=0.5\n"
		"Accept-Encoding: gzip, deflate\n"
		"Connection: keep-alive\n\n";
	size_t request_length = strlen(request);

	std::unordered_map<int, int> client_data;

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("connections", &connections, {"-c"});
	Util::define_argument("requests", &requests, {"-r"});
	Util::define_argument("verbose", &verbose, {"-v"});
	Util::parse_arguments(argc, argv, "This program will make threads for connections, each making a bunch of requests.");

	EventClient client;

	for(int i = 0; i < connections; ++i){
		client.add(hostname, static_cast<uint16_t>(port));
	}

	client.on_connect = [&](int fd){
		if(write(fd, request, request_length) < 0){
			ERROR("write")
			return true;
		}
		client_data[fd] = 1;
		return false;
	};

	client.run([&](int fd, const char* data, size_t data_length)->ssize_t{
		if(verbose){
			PRINT("Response: " << data)
		}
		if(client_data[fd] >= requests){
			return -1;
		}
		if(write(fd, request, request_length) < 0){
			ERROR("write")
			return -1;
		}
		client_data[fd]++;
		return static_cast<ssize_t>(data_length);
	});		

	PRINT("DONE!")

	return 0;
}
