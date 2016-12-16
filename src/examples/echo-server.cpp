#include "util.hpp"
#include "tcp-event-server.hpp"

int main(int argc, char** argv){
	int max_connections = 1;

	Util::define_argument("max_connections", &max_connections, {"-c"});
	Util::parse_arguments(argc, argv, "This program opens a non-blocking TCP server and handles incoming connections via an event loop.");

	EventServer server(80, static_cast<size_t>(max_connections));

	server.run([](int fd, char* packet, ssize_t length){
		if(write(fd, packet, static_cast<size_t>(length)) < 0){
			ERROR("write")
			return true;
		}
		return false;
	});

	return 0;
}
