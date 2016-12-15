#include "util.hpp"
#include "node.hpp"
#include "tcp-event-server.hpp"

EventServer::EventServer(uint16_t port, size_t new_max_connections, PacketReceivedFunction new_packet_received)
:name("EventServer"),
max_connections(new_max_connections),
next_fds(new Node<size_t>()),
packet_received(new_packet_received){
	for(size_t i = 0; i < this->max_connections; ++i){
		this->client_fds.push_back(-1);
		this->next_fds->push(i);
	}
	if((this->server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0){
		throw std::runtime_error(this->name + " socket");
	}
	int opt = 1;
	if(setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) < 0){
		throw std::runtime_error(this->name + " setsockopt");
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	if(bind(this->server_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0){
		throw std::runtime_error(this->name + " bind");
	}
	if(listen(this->server_fd, 512) < 0){
		throw std::runtime_error(this->name + " listen");
	}
	PRINT(this->name << " listening on " << port)
}

// Should not return?
void EventServer::run(){
	int new_client_fd;
	bool running = true, close_connection;
	ssize_t rlen;
	char packet[PACKET_LIMIT];

	while(running){
	//	while(1){
			if(this->next_fds->value > 0){
				if((new_client_fd = accept(this->server_fd, 0, 0)) < 0){
					if(errno != EWOULDBLOCK){
						ERROR("accept")
						running = false;
					}
					// Nothing to accept = ^_^
	//				break;
				}else{
					std::cout << this->next_fds->value << " -> ";
					this->client_fds[this->next_fds->pop()] = new_client_fd;
					PRINT(this->next_fds->value)
				}
			}else{
	//			break;
			}
	//	}
		for(size_t i = 0; i < this->max_connections; ++i){
			if(this->client_fds[i] != -1){
				close_connection = false;
	//			while(1){
					if((rlen = read(this->client_fds[i], packet, PACKET_LIMIT)) < 0){
						if(errno != EWOULDBLOCK){
							ERROR("read")
							close_connection = true;
						}
						// Nothing to read = ^_^
	//					break;
					}else if(rlen == 0){
						// Closed connection?
						close_connection = true;
	//					break;
					}else{
						close_connection = this->packet_received(this->client_fds[i], packet, rlen);
					}
	//			}
				if(close_connection){
					if(close(this->client_fds[i]) < 0){
						ERROR("close")
					}
					PRINT(i << " done!")
					this->client_fds[i] = -1;
					this->next_fds->push(i);
				}
			}
		}
	}

	if(close(this->server_fd) < 0){
		ERROR("close server_fd")
	}
	for(size_t i = 0; i < this->max_connections; ++i){
		if(this->client_fds[i] != -1){
			if(close(this->client_fds[i]) < 0){
				ERROR("close")
			}
		}
	}
}
