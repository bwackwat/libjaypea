#include "json.hpp"
#include "util.hpp"
#include "websocket-server.hpp"
#include "tls-websocket-server.hpp"

class Player{
public:
	std::string secret;
	int x;
	int y;
	int gold;
	
	Player(std::string new_secret)
	:secret(new_secret){
		
	}
};

int main(int argc, char** argv){
	std::string ssl_certificate;
	std::string ssl_private_key;
	int port;
	bool http = false;

	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("https_port", &port, {"-p"});
	Util::define_argument("http", &http);
	Util::parse_arguments(argc, argv, "This is a chat server.");
	
	std::unordered_map<std::string, Player*> player_data;
	std::unordered_map<int /* fd */, std::string /* logged in handle */> client_player;

	EpollServer* server;

	if(http){
		server = new WebsocketServer(static_cast<uint16_t>(port), 10);
	}else{
		server = new TlsWebsocketServer(ssl_certificate, ssl_private_key, static_cast<uint16_t>(port), 10);
	}

	server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		JsonObject msg;
		msg.parse(data);

		JsonObject response(OBJECT);
		PRINT("CHAT ONREAD")

		if(msg.HasObj("handle", STRING) &&
		msg.HasObj("message", STRING)){
			if(msg.GetStr("handle").length() < 5){
				response.objectValues["status"] = new JsonObject("Handle too short.");
			}else if(msg.GetStr("handle").length() > 16){
				response.objectValues["status"] = new JsonObject("Handle too long.");
			}else if(msg.GetStr("message").length() < 2){
				response.objectValues["status"] = new JsonObject("Message too short.");
			}else if(msg.GetStr("message").length() > 256){
				response.objectValues["status"] = new JsonObject("Message too long.");
			}else{
				response.objectValues["status"] = new JsonObject("Sent.");
				PRINT("TRY BROADCAST")
				if(server->EpollServer::send(server->broadcast_fd(), data, static_cast<size_t>(data_length))){
					PRINT("broadcast fail")
					return -1;
				}
			}
		}else{
			response.objectValues["status"] = new JsonObject("Must provide handle and message.");
		}
		std::string sresponse = response.stringify(false);
		PRINT("SEND:" << sresponse)
		if(server->send(fd, sresponse.c_str(), static_cast<size_t>(sresponse.length()))){
			PRINT("send prob")
			return -1;
		}
		return data_length;
	};

	server->run(false, 1);
}
