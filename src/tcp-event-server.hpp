#include <stack>
#include <vector>

class EventServer{
private:
	std::string name;
	unsigned long max_connections;

	int server_fd;
	std::stack<size_t> next_fd;
	std::vector<int> client_fds;
public:
	EventServer(uint16_t port, unsigned long new_max_connections);
	void run();
};
