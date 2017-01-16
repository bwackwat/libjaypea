#include "symmetric-tcp-client.hpp"

SymmetricTcpClient::SymmetricTcpClient(std::string ip_address, uint16_t port, std::string keyfile)
:SimpleTcpClient(ip_address, port),
encryptor(keyfile){}

void SymmetricTcpClient::reconnect(){
	this->writes = 0;
	this->reads = 0;
	SimpleTcpClient::reconnect();
}

std::string SymmetricTcpClient::communicate(const char* request, size_t length){
	char response[PACKET_LIMIT];

	this->comm_mutex.lock();

	do{

		while(!this->connected){
			this->reconnect();
		}

		ssize_t len;
		if(this->encryptor.send(this->fd, request, length, this->writes++)){
			ERROR("send")
			this->connected = false;
		}else if((len = this->encryptor.recv(this->fd, response, PACKET_LIMIT, nullptr, this->reads++)) <= 0){
			ERROR("recv")
			this->connected = false;
		}

	}while(!this->connected);

	this->comm_mutex.unlock();

	return std::string(response);
}
