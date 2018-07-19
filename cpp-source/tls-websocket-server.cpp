#include <functional>

#include "util.hpp"
#include "tls-websocket-server.hpp"

TlsWebsocketServer::TlsWebsocketServer(std::string certificate, std::string private_key, uint16_t port, size_t max_connections)
:TlsEpollServer(certificate, private_key, port, max_connections, "TlsWebsocketServer"),
websocket(this){}

bool TlsWebsocketServer::send(int fd, const char* data, size_t data_length){
	if(this->websocket.client_handshake_complete[fd]){
		std::string frame = this->websocket.create_frame(data, data_length);
		return TlsEpollServer::send(fd, frame.c_str(), frame.length());
	}else{
		return TlsEpollServer::send(fd, data, data_length);
	}
}

ssize_t TlsWebsocketServer::recv(int fd, char* data, size_t data_length){
	return TlsEpollServer::recv(fd, data, data_length,
		std::bind(&Websocket::recv, &this->websocket,
			std::placeholders::_1,
			std::placeholders::_2,
			std::placeholders::_3));
}

bool TlsWebsocketServer::accept_continuation(int* fd){
	this->websocket.client_handshake_complete[*fd] = false;
	return TlsEpollServer::accept_continuation(fd);
}
