#include "util.hpp"
#include "stack.hpp"
#include "tcp-event-server.hpp"

EventServer::EventServer(uint16_t port, size_t new_max_connections, std::string new_name)
:name(new_name),
max_connections(new_max_connections),
running(true){
	if((this->server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		throw std::runtime_error(this->name + " socket");
	}
/*
	For this class, I am unsure about what this is genuinely useful for...
	However, it has been useful in the past and I have read it as "good practice."
*/
	int opt = 1;
	if(setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) < 0){
		perror("setsockopt");
		throw std::runtime_error(this->name + " setsockopt");
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	if(bind(this->server_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0){
		perror("bind");
		throw std::runtime_error(this->name + " bind");
	}
	if(listen(this->server_fd, 1024) < 0){
		perror("listen");
		throw std::runtime_error(this->name + " listen");
	}
	PRINT(this->name << " listening on " << port)
}

void EventServer::start_event(Event* event){
/*
	for(unsigned int i = 0; i < this->num_threads; ++i){
		this->thread_event_starting_queues[i].enqueue(event);
	}
	for(unsigned int i = 0; i < this->num_threads; ++i){
		if(this->thread_event_queue_mutexes[i].try_lock()){
			while(this->thread_event_starting_queues[i].size > 0){
				this->thread_event_queues[i].enqueue(this->thread_event_starting_queues[i].dequeue());
			}
			this->thread_event_queue_mutexes[i].unlock();
		}
	}
*/

	for(unsigned int i = 0; i < this->num_threads; ++i){
		this->thread_event_queue_mutexes[i].lock();
		this->thread_event_queues[i].enqueue(event);
		this->thread_event_queue_mutexes[i].unlock();
	}
}

void EventServer::close_client(size_t index, int* fd, std::function<void(size_t, int*)> callback){
	callback(index, fd);
}

bool EventServer::send(int fd, const char* data, size_t data_length){
	ssize_t len;
	if((len = write(fd, data, data_length)) < 0){
		perror("write");
		ERROR("send")
		return true;
	}
	if(static_cast<size_t>(len) != data_length){
		ERROR("NOT ALL THE DATA WAS SENT TO " << fd)
	}
	this->write_counter[fd]++;
	return false;
}

ssize_t EventServer::recv(int fd, char* data, size_t data_length){
	ssize_t len;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK && errno != EAGAIN){
			perror("read");
			ERROR(this->name << " on " << fd)
			return -1;
		}
		return 0;
	}else if(len == 0){
		ERROR("server read zero on " << fd)
		return -2;
	}else{
		//PRINT("server read " << len << " on " << fd)
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
		*new_client_fd = 0;
		return true;
	}
	this->read_counter[*new_client_fd] = 0;
	this->write_counter[*new_client_fd] = 0;
	return this->accept_continuation(new_client_fd);
}

bool EventServer::accept_continuation(int* new_client_fd){
	Util::set_non_blocking(*new_client_fd);
	if(this->on_connect != nullptr){
		this->on_connect(*new_client_fd);
	}
	return true;
}

void EventServer::run_thread(unsigned int id){
	Stack<size_t> next_fd_indexes;
	std::vector<int> client_fds;
	size_t new_fd_index;
	ssize_t len;
	int new_fd, num_connections = 0, connections_done;
	char packet[PACKET_LIMIT];
	Event* next_event;

	std::function<void(size_t, int*)> close_client_callback = [&](size_t index, int* fd){
		if(close(*fd) < 0){
			ERROR("close_client")
		}
		PRINT(*fd << " done " << " on T " << id)
		if(this->on_disconnect != nullptr){
			this->on_disconnect(*fd);
		}
		*fd = -1;
		next_fd_indexes.push(index);
		num_connections--;
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
							PRINT("bcast " << client_fds[i])
							if(this->send(client_fds[i], next_event->data, next_event->data_length)){
								ERROR("broadcast send")
								this->close_client(i, &client_fds[i], close_client_callback);
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
				if(new_fd > 0){
					new_fd_index = next_fd_indexes.pop();
					client_fds[new_fd_index] = new_fd;
					PRINT("accepted " << new_fd_index << " from " << new_fd << " on " << id)
					if(++this->accepting_id >= this->num_threads){
						this->accepting_id = 0;
					}
					num_connections++;
				}else if(new_fd == -1){
					new_fd_index = next_fd_indexes.pop();
					client_fds[new_fd_index] = new_fd;
					PRINT("-1 closing: accepted " << new_fd_index << " from " << new_fd)
					num_connections++;
					this->close_client(new_fd_index, &client_fds[new_fd_index], close_client_callback);
				}else if(new_fd == 0){
					//Nothing to accept
				}else{
					ERROR("GOT STRANGE NEW FD" << new_fd)
				}
			}
		}
		if(num_connections > 0){
			connections_done = 0;
			for(size_t i = 0; i < this->max_connections; ++i){
				if(client_fds[i] != -1){
					if((len = this->recv(client_fds[i], packet, PACKET_LIMIT)) < 0){
						this->close_client(i, &client_fds[i], close_client_callback);
					}else if(len > 0){
						this->read_counter[client_fds[i]]++;
					}
					if(++connections_done >= num_connections){
						break;
					}
				}
			}
		}
	}

	PRINT(this->name << " thread " << id << " is done!")
}

void EventServer::run(bool returning, unsigned int new_num_threads){
	this->num_threads = new_num_threads;
	if(this->num_threads <= 0){
		this->num_threads = 1;
	}
	thread_event_queues = new Queue<Event*>[this->num_threads];
	thread_event_queue_mutexes = new std::mutex[this->num_threads];

	unsigned int total = this->num_threads;
	if(!returning){
		total--;
	}

	for(unsigned int i = 0; i < total; ++i){
			std::thread next(&EventServer::run_thread, this, i);
			next.detach();
	}

	if(returning){
		return;
	}

	this->run_thread(total);

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
	}
*/
}
