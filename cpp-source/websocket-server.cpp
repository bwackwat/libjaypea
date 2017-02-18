#include <functional>

#include "util.hpp"
#include "tcp-server.hpp"
#include "websocket-server.hpp"

WebsocketServer::WebsocketServer(uint16_t port, size_t max_connections)
:EpollServer(port, max_connections, "WebsocketServer"),
websocket(this){}

bool WebsocketServer::send(int fd, const char* data, size_t data_length){
	if(this->websocket.client_handshake_complete[fd]){
		std::string frame = this->websocket.create_frame(data, data_length);
		return EpollServer::send(fd, frame.c_str(), frame.length());
	}else{
		return EpollServer::send(fd, data, data_length);
	}
}

ssize_t WebsocketServer::recv(int fd, char* data, size_t data_length){
	return EpollServer::recv(fd, data, data_length,
		std::bind(&Websocket::recv, &this->websocket,
			std::placeholders::_1,
			std::placeholders::_2,
			std::placeholders::_3));
}

bool WebsocketServer::accept_continuation(int* fd){
	this->websocket.client_handshake_complete[*fd] = false;
	return false;
}
