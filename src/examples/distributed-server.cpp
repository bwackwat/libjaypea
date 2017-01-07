#include "tcp-event-server.hpp"

int main(){
	int port = 12345;

	EpollServer server(static_cast<uint16_t>(port), 10);

