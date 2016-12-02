#include "util.hpp"

int main(int argc, char** argv){
	int serverfd, clientfd, num_clients = 1, num = 1;
	long rlen = 1, wlen;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	uint addr_len = sizeof(client_addr);
	bool exit = false, close_connection;

	char packet[PACKET_LIMIT];
	struct pollfd clients[CONNECTIONS_LIMIT];

	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "-h") == 0){
			PRINT("https://github.com/bwackwat/event-spiral")
		}
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if((serverfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0){
		ERROR("socket")
		return 1;
	}

	if(setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&num), sizeof(num)) < 0){
		ERROR("setsockopt")
		return 1;
	}

	if(bind(serverfd, reinterpret_cast<struct sockaddr*>(&server_addr), addr_len) < 0){
		ERROR("bind")
		return 1;
	}

	if(listen(serverfd, 32) < 0){
		ERROR("listen")
		return 1;
	}

	memset(clients, 0, sizeof(clients));
	clients[0].fd = serverfd;
	clients[0].events = POLLIN;

	while(1){
		DEBUG("poll loop")
		DEBUG_SLEEP(1)

		// The timeout is -1 (milliseconds), so poll will wait forever if nothing is happening.
		// Poll returns 0 on timeout, otherwise returns the number of readable descriptors.
		if(poll(clients, static_cast<nfds_t>(num_clients), -1) < 0){
			ERROR("poll")
			break;
		}

		num = num_clients;
		for(int i = 0; i < num; ++i){
			if(clients[i].revents == 0){
				continue;
			}
			if(clients[i].fd == serverfd){
				while(1){
					if((clientfd = accept(serverfd, 0, 0)) < 0){
						// If errno is EWOULDBLOCK there is nthing to accept.
						if(errno != EWOULDBLOCK){
							ERROR("accept")
							exit = true;
						}
						break;
					}
					// TODO Use fds that have been set to -1;
					int next = 0;
					while(1){
						if(next == CONNECTIONS_LIMIT){
							PRINT("Max connections reached!")
							break;
						}else if(clients[next].fd == -1){
							break;
						}else if(next == num_clients){
							num_clients++;
							break;
						}
						++next;
					}
					if(next != CONNECTIONS_LIMIT){
						clients[next].fd = clientfd;
						clients[next].events = POLLIN;
					}
				}
			}else{
				close_connection = false;
				while(1){
					if((rlen = read(clients[i].fd, packet, PACKET_LIMIT)) < 0){
						if(errno != EWOULDBLOCK){
							ERROR("read")
							close_connection = true;
						}
						break;
					}
					if(rlen == 0){
						// connection closed on other end?
						close_connection = true;
						break;
					}
					// Echo..
					if((wlen = write(clients[i].fd, packet, static_cast<size_t>(rlen))) < 0){
						ERROR("write")
						close_connection = true;
						break;
					}
				}
				if(close_connection){
					if(close(clients[i].fd) < 0){
						std::cout << "Uh oh, close error." << std::endl;
					}
					clients[i].fd = -1;
				}
			}
		}
		if(exit){
			break;
		}
	}

	for(int i = 0; i < num_clients; ++i){
		if(clients[i].fd >= 0){
			if(close(clients[i].fd) < 0){
				std::cout << "Uh oh, ending close error." << std::endl;
			}
		}
	}

	return 0;
}
