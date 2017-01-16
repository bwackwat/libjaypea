#include <thread>
#include <vector>
#include <cstring>
#include <stack>

#include "util.hpp"

static int requests = 1;
static bool verbose = false;

static const char* request = "GET / HTTP/1.1\n"
"User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:45.0) Gecko/20100101 Firefox/45.0\n"
"Accept: image/png,image/*;q=0.8,*/*;q=0.5\n"
"Accept-Language: en-US,en;q=0.5\n"
"Accept-Encoding: gzip, deflate\n"
"Connection: keep-alive\n\n";
static size_t request_length = strlen(request);

int main(int argc, char** argv){
	std::string hostname = "localhost";
	int port, connections = 1, num_connected = 0, num_completed = 0, num_clients = 0, res;
	ssize_t len;
	struct hostent* host;

	bool close_connection;
	char packet[PACKET_LIMIT];
	struct pollfd clients[CONNECTIONS_LIMIT];
	std::stack<size_t> next_client;
	for(size_t i = 0; i < CONNECTIONS_LIMIT; ++i){
		next_client.push(i);
	}

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("connections", &connections, {"-c"});
	Util::define_argument("requests", &requests, {"-r"});
	Util::define_argument("verbose", &verbose, {"-v"});
	Util::parse_arguments(argc, argv, "This program will make threads for connections, each making a bunch of requests.");

	if((host = gethostbyname(hostname.c_str())) == 0){
		ERROR("gethostbyname")
		return 1;
	}
	struct in_addr host_addr = *reinterpret_cast<struct in_addr*>(host->h_addr);

	memset(clients, 0, sizeof(clients));

	std::map<int, int> request_count;

	while(1){
		if(num_connected < connections){
			if(next_client.size() > 0){
				int next_fd;
				struct sockaddr_in server_addr;
				server_addr.sin_family = AF_INET;
				server_addr.sin_addr = host_addr;
				server_addr.sin_port = htons(static_cast<uint16_t>(port));
				bzero(&(server_addr.sin_zero), 8);
				if((next_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
					throw std::runtime_error("new socket");
				}
				if(connect(next_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(struct sockaddr_in)) < 0){
					throw std::runtime_error("new connect");
				}
				clients[next_client.top()].fd = next_fd;
				clients[next_client.top()].events = POLLIN;
				next_client.pop();
				if(write(next_fd, request, request_length) < 0){
					throw std::runtime_error("Initial client request!");
				}
				request_count[next_fd] = 1;
				num_connected++;
			}
		}
		// No timeout.
		if((res = poll(clients, static_cast<nfds_t>(num_clients), 0)) < 0){
			ERROR("poll")
			break;
		}else if(res > 0){
			for(size_t i = 0; i < static_cast<size_t>(num_clients); ++i){
				if(clients[i].fd == -1 || clients[i].revents == 0){
					continue;
				}
				res--;
				close_connection = false;
				if((len = read(clients[i].fd, packet, PACKET_LIMIT)) < 0){
					ERROR("read")
					close_connection = true;
				}else if(len == 0){
					ERROR("read 0 bytes, closed on server?")
					close_connection = true;
				}else if(request_count[clients[i].fd] < requests){
					PRINT("got " << packet)
					if(write(clients[i].fd, request, request_length) < 0){
						throw std::runtime_error("Client request" + std::to_string(request_count[clients[i].fd]));
					}
					request_count[clients[i].fd]++;
				}else{
					close_connection = true;
				}
				if(close_connection){
					if(close(clients[i].fd) < 0){
						ERROR("close")
					}
					clients[i].fd = -1;
					next_client.push(i);
					num_completed++;
					PRINT("Connection #" << num_completed << " done.")
				}
				if(res == 0){
					break;
				}
			}
		}
		if(num_connected >= connections && num_completed >= connections){
			break;
		}
	}

	PRINT("DONE!")

	return 0;
}
