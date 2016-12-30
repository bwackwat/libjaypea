#include "util.hpp"
#include "stack.hpp"
#include "tcp-event-server.hpp"

EventServer::EventServer(std::string new_name, uint16_t port, size_t new_max_connections)
:name(new_name),
max_connections(new_max_connections),
num_threads(std::thread::hardware_concurrency()),
running(true){
	if(this->num_threads == 0){
		this->num_threads = 1;
	}
	thread_event_queues = new Queue<Event*>[this->num_threads];
	thread_event_queue_mutexes = new std::mutex[this->num_threads];

	if((this->server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		throw std::runtime_error(this->name + " socket");
	}
	Util::set_non_blocking(this->server_fd);
/*
	For this class, I am unsure about what this is genuinely useful for...
	However, it has been useful in the past and I have read it as "good practice."
*/
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

void EventServer::start_event(Event* event){
	for(unsigned int i = 0; i < this->num_threads; ++i){
		this->thread_event_queue_mutexes[i].lock();
		this->thread_event_queues[i].enqueue(event);
		this->thread_event_queue_mutexes[i].unlock();
	}
}

void EventServer::close_client(size_t index, std::function<void(size_t)> callback){
	callback(index);
}

bool EventServer::send(int fd, const char* data, size_t data_length){
	if(write(fd, data, data_length) < 0){
		ERROR("send")
		return true;
	}
	return false;
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
		return this->on_read(fd, data, len);
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
		if(this->on_connect != nullptr){
			this->on_connect(*new_client_fd);
		}
	}
	return true;
}

void EventServer::run_thread(unsigned int id){
	Stack<size_t> next_fd_indexes;
	std::vector<int> client_fds;
	int new_fd;
	char packet[PACKET_LIMIT];
	Event* next_event;

	std::function<void(size_t)> close_client_callback = [&](size_t index){
		if(close(client_fds[index]) < 0){
			ERROR("close_client")
		}
		PRINT(index << " done!")
		int old_fd = client_fds[index];
		client_fds[index] = -1;
		next_fd_indexes.push(index);
		if(this->on_disconnect != nullptr){
			this->on_disconnect(old_fd);
		}
	};

	for(size_t i = 0; i < this->max_connections; ++i){
		client_fds.push_back(-1);
		next_fd_indexes.push(i);
	}
	
	PRINT(this->name << " thread " << id << " is starting!")

	while(this->running){
		if(this->thread_event_queues[id].size > 0){
			if(this->thread_event_queue_mutexes[id].try_lock()){
				next_event = this->thread_event_queues[id].dequeue();
				this->thread_event_queue_mutexes[id].unlock();
				switch(next_event->type){
				case BROADCAST:
					for(size_t i = 0; i < this->max_connections; ++i){
						if(client_fds[i] != -1){
							if(this->send(client_fds[i], next_event->data, next_event->data_length)){
								ERROR("broadcast send")
								this->close_client(i, close_client_callback);
							}
						}
					}
					break;
				}
			}
		}
		if(this->accepting_id == id){
			if(next_fd_indexes.size > 0){
				this->running = this->non_blocking_accept(&new_fd);
				if(new_fd >= 0){
					client_fds[next_fd_indexes.pop()] = new_fd;
					PRINT("accepted " << new_fd << " from " << id)
					if(this->accepting_id >= this->num_threads){
						this->accepting_id = 0;
					}else{
						this->accepting_id++;
					}
				}
			}
		}
		for(size_t i = 0; i < this->max_connections; ++i){
			if(client_fds[i] != -1){
				if(this->recv(client_fds[i], packet, PACKET_LIMIT)){
					this->close_client(i, close_client_callback);
				}
			}
		}
	}

	PRINT(this->name << " thread " << id << " is done!")
}

// Should not return?
void EventServer::run(bool returning){
	unsigned int i = 0;
	if(returning){
		i++;
	}

	for(; i < this->num_threads; ++i){
			std::thread next(&EventServer::run_thread, this, i);
			next.detach();
	}

	if(returning){
		return;
	}

	this->run_thread(this->num_threads);

/*
	if(close(this->server_fd) < 0){
		ERROR("close server_fd")
	}
	for(size_t i = 0; i < this->max_connections; ++i){
		if(this->client_fds[i] != -1){
			if(close(this->client_fds[i]) < 0){
				ERROR("close")
			}
		}
	}*/
}
