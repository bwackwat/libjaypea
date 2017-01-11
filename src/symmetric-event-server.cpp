#include <sys/types.h>
#include <fcntl.h>

#include "util.hpp"
#include "symmetric-event-server.hpp"

SymmetricEventServer::SymmetricEventServer(std::string keyfile, uint16_t port, size_t new_max_connections)
:EpollServer(port, new_max_connections, "SymmetricEventServer"),
encryptor(keyfile){}

bool SymmetricEventServer::send(int fd, const char* data, size_t data_length){
	return this->encryptor.send(fd, data, data_length, this->write_counter[fd]++);
}

ssize_t SymmetricEventServer::recv(int fd, char* data, size_t data_length){
	return this->encryptor.recv(fd, data, data_length, this->on_read, this->read_counter[fd]);
}
