#include "util.hpp"
#include "node.hpp"
#include "tcp-event-server.hpp"

EventServer::EventServer(std::string new_name, uint16_t port, size_t new_max_connections)
:name(new_name),
max_connections(new_max_connections),
next_fds(new Node<size_t>()){
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
		ERROR("server read zero")
		return true;
	}else{
		data[len] = 0;
		return packet_received(fd, data, len);
	}
}

bool EventServer::non_blocking_accept(int* new_client_fd){
	if((*new_client_fd = accept(this->server_fd, 0, 0)) < 0){
		if(errno != EWOULDBLOCK){
			ERROR("accept")
			return false;
		}
		// Nothing to accept = ^_^
	}else{
		Util::set_non_blocking(*new_client_fd);
		this->client_fds[this->next_fds->pop()] = new_client_fd;
		if(this->on_connect != nullptr){
			this->on_connect(new_client_fd);
		}
	}
	return true;
}

void EventServer::run_thread(int id, StackNode<){
	Node<size_t>* next_fd_indexes;
	std::vector<int> client_fds;
	int new_fd;
	char packet[PACKET_LIMIT];

	for(size_t i = 0; i < this->max_connections; ++i){
		client_fds.push_back(-1);
		next_fd_indexes->push(i);
	}
	
	PRINT(this->name << " thread " << id << " is starting!")

	while(this->running){
		if(this->accepting_id == id){
			if(this->num_connections < this->max_connections){
				this->running = this->nonblocking_accept(&new_fd);
				if(new_fd >= 0){
					client_fds[this->next_fds->pop()] = new_fd;
				}
			}
		}
		for(size_t i = 0; i < this->max_connections; ++i){
			if(client_fds[i] != -1){
				if(this->recv(client_fds[i], packet, PACKET_LIMIT)){
					this->close_client(i);
				}
			}
		}
	}

	PRINT(this->name << " thread " << id << " is done!")
}

// Should not return?
void EventServer::run(bool returning){
	unsigned int threads = std::thread::hardware_concurrency();
	if(threads == 0){
		threads = 1;
	}

	if(returning){
		threads--;
	}

	for(unsigned int i = 0; i < threads; ++i){
			std::thread next(&EventServer::run_thread, this, i);
			next.detach();
	}

	if(returning){
		return;
	}

	this->run_thread(threads);

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
