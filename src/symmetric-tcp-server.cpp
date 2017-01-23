#include <sys/types.h>
#include <fcntl.h>

#include "util.hpp"
#include "symmetric-tcp-server.hpp"

SymmetricEpollServer::SymmetricEpollServer(std::string keyfile, uint16_t port, size_t new_max_connections)
:EpollServer(port, new_max_connections, "SymmetricEpollServer"),
encryptor(keyfile){}

bool SymmetricEpollServer::send(int fd, const char* data, size_t data_length){
	return this->encryptor.send(fd, data, data_length, this->write_counter[fd]++);
}

ssize_t SymmetricEpollServer::recv(int fd, char* data, size_t data_length){
	return this->encryptor.recv(fd, data, data_length, this->on_read, this->read_counter[fd]);
}
