#include <unordered_map>

#include "tcp-server.hpp"
#include "json.hpp"
#include "util.hpp"

static const char* clean(std::string string){
	std::stringstream cleaned;
	for(char& c : string){
		if(c > 31){
			cleaned << c;
		}
	}
	PRINT("GOT " << cleaned.str().c_str())
	return cleaned.str().c_str();
}

int main(int argc, char** argv){
	int port;

	Util::define_argument("port", &port, {"-p"});
	Util::parse_arguments(argc, argv, "Chat server");

	JsonObject packet_data(OBJECT);
	packet_data.objectValues["type"] = new JsonObject(std::string());
	packet_data.objectValues["message"] = new JsonObject(std::string());
	packet_data.objectValues["handle"] = new JsonObject(std::string());
	std::string message;

	std::unordered_map<int, std::string> client_data;
	EpollServer server(static_cast<uint16_t>(port), 100);

	server.on_connect = [&](int fd){
		client_data[fd] = std::string();
	};

	server.on_read = [&](int fd, const char* packet, size_t data_length)->ssize_t{
		packet_data["type"]->stringValue = std::string();
		packet_data["message"]->stringValue = std::string();
		packet_data["handle"]->stringValue = std::string();
		packet_data.parse(packet);

		if(std::strcmp(packet_data["type"]->stringValue.c_str(), "enter") == 0){
			client_data[fd] = packet_data["handle"]->stringValue;
			if(client_data[fd].length() < 5){
				message = "Your handle is less than 5 characters... bye!";
				server.send(fd, message.c_str(), message.length());
				return -1;
			}
			message = client_data[fd] + " has connected.";
		}else if(std::strcmp(packet_data["type"]->stringValue.c_str(), "message") == 0){
			if(packet_data["message"]->stringValue.length() > 128){
				message = "Messages longer than 128 characters are truncated, sorry.\n";
				if(server.send(fd, message.c_str(), message.length())){
					return -1;
				}
				packet_data["message"]->stringValue.resize(128);
			}
			message = client_data[fd] + " > " + clean(packet_data["message"]->stringValue.c_str());
		}else{
			message = "You provided an invalid packet type... bye!";
			server.send(fd, message.c_str(), message.length());
			return -1;
		}

		if(server.send(server.broadcast_fd(), message.c_str(), message.length())){
			ERROR("write to broadcast fd failed")
		}
		return static_cast<ssize_t>(data_length);
	};

	server.on_disconnect = [&](int fd){
		message = client_data[fd] + " has disconnected.";
		if(server.send(server.broadcast_fd(), message.c_str(), message.length())){
			ERROR("write to broadcast fd disconnect failed")
		}
	};

	server.run(false, 1);

	return 0;
}
