#include "util.hpp"
#include "node.hpp"
#include "tcp-event-server.hpp"

EventServer::EventServer(std::string new_name, uint16_t port, size_t new_max_connections)
:name(new_name),
max_connections(new_max_connections),
next_fds(new Node<size_t>()){
	for(size_t i = 0; i < this->max_connections; ++i){
		this->client_fds.push_back(-1);
	this->next_fds->push(i);
	}
	if((this->server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		throw std::runtime_error(this->name + " socket");
	}
	Util::set_non_blocking(this->server_fd);
//	For this class, I am unsure about what this is genuinely useful for...
//	However, it has been useful in the past and I have read it as "good practice."
//
//	int opt = 1;
//	if(setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) < 0){
//		throw std::runtime_error(this->name + " setsockopt");
//	}
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

void EventServer::broadcast(const char* packet, size_t length){
	for(size_t i = 0; i < this->max_connections; ++i){
		if(this->client_fds[i] != -1){
			if(write(this->client_fds[i], packet, length) < 0){
				ERROR("broadcast write")
				this->close_client(i);
			}
		}
	}
}

void EventServer::close_client(size_t index){
	if(close(this->client_fds[index]) < 0){
		ERROR("close_client")
	}
	PRINT(index << " done!")
	int old_fd = this->client_fds[index];
	this->client_fds[index] = -1;
	this->next_fds->push(index);
	if(this->on_disconnect != nullptr){
		this->on_disconnect(old_fd);
	}
}

bool EventServer::recv(int fd, char* data, size_t data_length){
	ssize_t len;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK){
			ERROR(this->name << " read")
			return true;
		}
		return false;
	}else if(len == 0){
		return true;
	}else{
		data[len] = 0;
		return packet_received(fd, data, len);
	}
}

bool EventServer::nonblocking_accept(){
	int new_client_fd;
	if((new_client_fd = accept(this->server_fd, 0, 0)) < 0){
		if(errno != EWOULDBLOCK){
			ERROR("accept")
			return false;
		}
		// Nothing to accept = ^_^
	}else{
		std::cout << this->next_fds->value << " -> ";
		Util::set_non_blocking(new_client_fd);
		this->client_fds[this->next_fds->pop()] = new_client_fd;
		PRINT(this->next_fds->value)
		if(this->on_connect != nullptr){
			this->on_connect(new_client_fd);
		}
	}
	return true;
}

// Should not return?
void EventServer::run(PacketReceivedFunction new_packet_received){
	bool running = true;
	char packet[PACKET_LIMIT];

	this->packet_received = new_packet_received;

	while(running){
		if(this->on_event_loop != nullptr){
			if(this->on_event_loop()){
				break;
			}
		}
	//	while(1){
			if(this->next_fds->value > 0){
				running = this->nonblocking_accept();
			}else{
	//			break;
			}
	//	}
		for(size_t i = 0; i < this->max_connections; ++i){
			if(this->client_fds[i] != -1){
				if(this->recv(this->client_fds[i], packet, PACKET_LIMIT)){
					this->close_client(i);
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
