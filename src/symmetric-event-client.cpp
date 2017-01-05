#include "symmetric-event-client.hpp"

SymmetricEventClient::SymmetricEventClient(std::string keyfile)
:encryptor(keyfile){}

bool SymmetricEventClient::send(int fd, const char* data, size_t data_length){
	return this->encryptor.send(fd, data, data_length);
}

bool SymmetricEventClient::recv(int fd, char* data, size_t data_length){
	return this->encryptor.recv(fd, data, data_length, this->on_read);
}
