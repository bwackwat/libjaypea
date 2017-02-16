#include <bitset>

#include "cryptopp/sha.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "json.hpp"
#include "util.hpp"
#include "tcp-server.hpp"
#include "websocket-server.hpp"

WebsocketServer::WebsocketServer(uint16_t port, size_t max_connections, std::string name)
:EpollServer(port, max_connections, name){}

bool WebsocketServer::accept_continuation(int* fd){
	this->client_handshaked[fd] = false;
}

bool WebsocketServer::send(int fd, const char* data, size_t data_length){
	std::string frame = Util::create_web_socket_frame(data, data_length);
	EpollServer::send(fd, frame);
}

ssize_t WebsocketServer::recv(int fd, char* data, size_t data_length){
	if(this->client_handshaked[fd]){
		ssize_t len = EpollServer::recv(fd, data, data_length);
		if(len <= 0){
			PRINT("Bad websocket receive.")
			return len;
		}else{
			std::string message = Util::parse_web_socket_frame(data, data_length);
			//TODO::
		}
	}else{
		JsonObject r_obj(OBJECT);
		Util::parse_http_api_request(data, &r_obj);
		PRINT("JPON: " << r_obj.stringify(true))
		if(r_obj.HasObj("Sec-WebSocket-Key", STRING)){
			const std::string& response = Util::get_websocket_handshake_response(r_obj.GetStr("Sec-WebSocket-Key"));
			if(EpollServer::send(fd, response.c_str(), response.length())){
				return -1;
			}
			PRINT("DELI: " << response)
			client_handshaked[fd] = true;
		}else{
			PRINT("Bad websocket request.")
			return -1;
		}
	}
	return data_length;
}
