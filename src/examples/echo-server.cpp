#include "util.hpp"
#include "tcp-epoll-server.hpp"
#include "tcp-event-server.hpp"

int main(int argc, char** argv){
	int port = 12345, max_connections = 1;
	bool verbose = false;

	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("max_connections", &max_connections, {"-c"});
	Util::define_argument("verbose", &verbose, {"-v"});
	Util::parse_arguments(argc, argv, "This program opens a non-blocking TCP server and handles incoming connections via an event loop.");

	EpollServer server(static_cast<uint16_t>(port), static_cast<size_t>(max_connections));

	server.on_read = [&](int fd, const char* packet, size_t length)->ssize_t{
		if(verbose){
			PRINT(packet)
		}
		if(server.send(fd, packet, length)){
			return -1;
		}
		return static_cast<ssize_t>(length);
	};

	server.run();

	return 0;
}
