#include "util.hpp"
#include "tcp-epoll-server.hpp"
#include "tcp-event-server.hpp"

int main(int argc, char** argv){
	int port, max_connections = 1;

	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("max_connections", &max_connections, {"-c"});
	Util::parse_arguments(argc, argv, "This program opens a non-blocking TCP server and handles incoming connections via an event loop.");

	EpollServer server(static_cast<uint16_t>(port), static_cast<size_t>(max_connections));

	server.on_read = [&](int fd, const char* packet, size_t length)->ssize_t{
		if(Util::verbose){
			PRINT(packet)
		}
		if(server.send(fd, packet, length)){
			return -1;
		}
		return static_cast<ssize_t>(length);
	};

	server.run(false, 1);

	return 0;
}
