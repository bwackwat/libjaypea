#include "symmetric-tcp-client.hpp"

SymmetricTcpClient::SymmetricTcpClient(std::string ip_address, uint16_t port, std::string keyfile)
:SimpleTcpClient(ip_address, port),
encryptor(keyfile){}

void SymmetricTcpClient::reconnect(){
	this->writes = 0;
	this->reads = 0;
	SimpleTcpClient->reconnect();
}

bool SymmetricTcpClient::communicate(const char* request, size_t length, char* response){
	ssize_t len;
	if(this->encryptor.send(this->fd, request, length, this->writes++)){
		return true;
	}
	len = this->encryptor.recv(this->fd, response, PACKET_LIMIT, nullptr);
	this->reads++;
	return len > 0;
}
