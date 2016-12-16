#include <unordered_map>

#include "tcp-event-server.hpp"
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

int main(){
	JsonObject packet_data(OBJECT);
	packet_data.objectValues["type"] = new JsonObject(std::string());
	packet_data.objectValues["message"] = new JsonObject(std::string());
	packet_data.objectValues["handle"] = new JsonObject(std::string());
	std::string message;

	std::unordered_map<int, std::string> client_data;
	EventServer server(10000, 1000);

	server.on_disconnect = [&](int fd){
		message = client_data[fd] + " has disconnected.";
		server.broadcast(message.c_str(), message.length());
	};

	server.run([&](int fd, char* packet, size_t){
		packet_data["type"]->stringValue = std::string();
		packet_data["message"]->stringValue = std::string();
		packet_data["handle"]->stringValue = std::string();
		packet_data.parse(packet);

		if(std::strcmp(packet_data["type"]->stringValue.c_str(), "enter") == 0){
			client_data[fd] = packet_data["handle"]->stringValue;
			if(client_data[fd].length() < 5){
				message = "Your handle is less than 5 characters... bye!";
				write(fd, message.c_str(), message.length());
				return true;
			}
			message = client_data[fd] + " has connected.";
		}else if(std::strcmp(packet_data["type"]->stringValue.c_str(), "message") == 0){
			if(packet_data["message"]->stringValue.length() > 128){
				message = "Messages longer than 128 characters are truncated, sorry.\n";
				if(write(fd, message.c_str(), message.length()) < 0){
					ERROR("write")
					return true;
				}
				packet_data["message"]->stringValue.resize(128);
			}
			message = client_data[fd] + " > " + clean(packet_data["message"]->stringValue.c_str());
		}else{
			message = "You provided an invalid packet type... bye!";
			write(fd, message.c_str(), message.length());
			return true;
		}

		server.broadcast(message.c_str(), message.length());
		return false;
	});

	return 0;
}
