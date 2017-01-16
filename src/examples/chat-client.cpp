#include <termios.h>

#include "simple-tcp-client.hpp"
#include "util.hpp"
#include "json.hpp"

#define ANONYMOUS "Anonymous"

/*
static struct termios saved_attributes;

static void reset_input_mode(){
	tcsetattr(0, TCSANOW, &saved_attributes);
}
*/

int main(int argc, char** argv){
	int port;
	std::string hostname;
	std::string handle = ANONYMOUS;
	ssize_t len;
	char packet[PACKET_LIMIT];
	char char_read;
	std::stringstream new_message;
	JsonObject packet_data(OBJECT);
	std::string packet_string;

	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("handle", handle);
	Util::parse_arguments(argc, argv, "This is a client for chat-server.");

	if(handle == ANONYMOUS){
		std::cout << "Enter your handle [" << ANONYMOUS << "]: ";
		std::getline(std::cin, handle);
		if(handle.empty()){
			handle = ANONYMOUS;
		}
	}

	SimpleTcpClient client(hostname, static_cast<uint16_t>(port));

	Util::set_non_blocking(0);
	Util::set_non_blocking(client.fd);
/*
	struct termios tattr;

	if(!isatty(0)){
		ERROR("isatty")
		return 1;
	}

	tcgetattr(0, &saved_attributes);
	atexit(reset_input_mode);

	tcgetattr(0, &tattr);
	tattr.c_lflag &= ~(ICANON | ECHO);
	tattr.c_cc[VMIN] = 1;
	tattr.c_cc[VTIME] = 0;
	tcsetattr (0, TCSAFLUSH, &tattr);
*/
	packet_data.objectValues["type"] = new JsonObject("enter");
	packet_data.objectValues["handle"] = new JsonObject(handle);
	packet_data.objectValues["message"] = new JsonObject(STRING);
	packet_string = packet_data.stringify();
	if(write(client.fd, packet_string.c_str(), packet_string.length()) < 0){
		ERROR("write")
	}

	std::cout << handle << " << " << std::flush;

	while(1){
		if((len = read(0, &char_read, 1)) < 0){
			if(errno != EWOULDBLOCK){
				ERROR("read stdin")
			}
		}else if(len == 0){
			ERROR("read 0 from stdin?")
		}else{
			if(char_read == '\n'){
				packet_data["type"]->stringValue = "message";
				packet_data["message"]->stringValue = new_message.str();
				packet_string = packet_data.stringify();
				if(write(client.fd, packet_string.c_str(), packet_string.length()) < 0){
					ERROR("write")
				}
				new_message.str(std::string());
			}else{
				new_message << char_read;
				std::cout << char_read << std::flush;
			}
		}
		if((len = read(client.fd, packet, PACKET_LIMIT)) < 0){
			if(errno != EWOULDBLOCK){
				ERROR("read client fd")
			}
		}else if(len == 0){
			ERROR("kicked from server")
			if(close(client.fd) < 0){
				ERROR("close")
			}
			break;
		}else{
			packet[len] = 0;
			std::cout << '\r' << std::flush << packet << std::endl;
			std::cout << handle << " << " << new_message.str() << std::flush;
		}
	}

	return 0;
}
