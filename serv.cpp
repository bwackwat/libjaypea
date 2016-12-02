#include <iostream>
#include <string>

#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PACKET_LIMIT 2048
#define CONNECTIONS_LIMIT 2048

int main(int argc, char** argv){
	int serverfd, clientfd, len = 1, num_clients = 1, num;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	uint addr_len = sizeof(client_addr);
	bool exit = false;

	char packet[PACKET_LIMIT];
	struct pollfd clients[CONNECTIONS_LIMIT];

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(1010);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		std::cout << "Uh oh, socket error." << std::endl;
		return 1;
	}

	if(setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (char*)&len, sizeof(len)) < 0){
		std::cout << "Uh oh, setsockopt error." << std::endl;
		return 1;
	}

	if(ioctl(serverfd, FIONBIO, (char*)&len) < 0){
		std::cout << "Uh oh, ioctl error." << std::endl;
		return 1;
	}

	if(bind(serverfd, (struct sockaddr*)&server_addr, addr_len) < 0){
		std::cout << "Uh oh, bind error." << std::endl;
		return 1;
	}

	if(listen(serverfd, 32) < 0){
		std::cout << "Uh oh, listen error." << std::endl;
		return 1;
	}

	memset(clients, 0, sizeof(clients));
	clients[0].fd = serverfd;
	clients[0].events = POLLIN;

	while(1){
		// The timeout is -1 (milliseconds), so poll will wait forever if nothing is happening.
		// Poll returns 0 on timeout, otherwise returns the number of readable descriptors.
		if(poll(clients, num_clients, -1) < 0){
			std::cout << "Uh oh, poll error." << std::endl;
			return 1;
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
							std::cout << "Uh oh, accept error." << std::endl;
							exit = true;
						}
						break;
					}
					// TODO Use fds that have been set to -1;
					clients[num_clients].fd = clientfd;
					clients[num_clients].events = POLLIN;
					num_clients++;
				}
			}else{
				close_connection = false;
				while(1){
					if((len = read(clients[i].fd, packet, PACKET_LIMIT)) < 0){
						if(errno != EWOULDBLOCK){
							std::cout << "Uh oh, read error." << std::endl;
							close_connection = true;
						}
						break;
					}
					if(len == 0){
						// connection closed on other end?
						close_connection = true;
						break;
					}
					// Echo..
					if((len = write(clients[i].fd, packet, len)) < 0){
						std::cout << "Uh oh, write error." << std::endl;
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
