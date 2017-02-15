#include <sys/types.h>
#include <fcntl.h>

#include "util.hpp"
#include "symmetric-tcp-server.hpp"

/**
 * /implements EpollServer
 *
 * @brief This class has the same characteristics of EpollServer, yet uses symmetric key encryption.
 *
 * @param keyfile The path to the keyfile made standard by @see SymmetricEncryptor.
 *
 * See EpollServer::EpollServer.
 *
 * This class encrypts written data and decrypts read data via @see SymmetricEpollServer::encryptor,
 * and keeps track of the number of writes and reads for transaction-based security.
 */
SymmetricEpollServer::SymmetricEpollServer(std::string keyfile, uint16_t port, size_t new_max_connections)
:EpollServer(port, new_max_connections, "SymmetricEpollServer"),
encryptor(keyfile){}

bool SymmetricEpollServer::send(int fd, std::string msg){
	return this->encryptor.send(fd, msg.c_str(), msg.length(), &this->write_counter[fd]);
}

bool SymmetricEpollServer::send(int fd, const char* data, size_t data_length){
	return this->encryptor.send(fd, data, data_length, &this->write_counter[fd]);
}

ssize_t SymmetricEpollServer::recv(int fd, char* data, size_t data_length){
	return this->encryptor.recv(fd, data, data_length, this->on_read, &this->read_counter[fd]);
}
