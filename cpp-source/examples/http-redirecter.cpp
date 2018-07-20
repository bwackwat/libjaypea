#include "util.hpp"
#include "tcp-server.hpp"

int main(int argc, char **argv){
	std::string hostname = "localhost";
	int port = 80;

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("port", &port, {"-p"});
	Util::parse_arguments(argc, argv, "This serves HTTP 301 Moved Permanently to an HTTPS hostname and port. Most browsers will automatically load the 301 \"Location\" header value as a URL.");

	EpollServer server(static_cast<uint16_t>(port), 10);

	std::string str = Util::get_redirection_html(hostname);
	const char* http_response = str.c_str();
	size_t http_response_length = str.length();

	server.on_read = [&](int fd, const char*, ssize_t)->ssize_t{
		server.send(fd, http_response, http_response_length);
		return -1;
	};
	
	server.run(false, 1);
}
