#include <sys/types.h>
#include <fcntl.h>

#include "util.hpp"
#include "symmetric-tcp-server.hpp"

/**
 * /implements EpollServer
 *
 * @brief This class has the same characteristics of EpollServer, yet uses AES256 bit symmetric key encryption.
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

bool SymmetricEpollServer::send(int fd, const char* data, size_t data_length){
	return this->encryptor.send(fd, data, data_length, this->write_counter[fd]++);

	std::string send_data = this->encryptor.encrypt(data, transaction);
	char block_size[4];
	Util::write_uint32_t(send_data.length(), block_size);
	ssize_t len;

	PRINT("SDATA|" << send_data.length() << '|')
	if((len = write(fd, block_size, 4)) < 0){
		ERROR("encryptor write")
		return true;
	}else if(len != 4){
		ERROR("encryptor didn't write all")
		return true;
	}

	PRINT("SDATA|" << send_data << '|')
	if((len = write(fd, send_data.c_str(), send_data.length())) < 0){
		ERROR("encryptor write block")
		return true;
	}else if(len != static_cast<ssize_t>(send_data.length())){
		ERROR("encryptor didn't write all block")
		return true;
	}
	return false;
}

ssize_t SymmetricEpollServer::recv(int fd, char* data, size_t data_length){
	return this->encryptor.recv(fd, data, data_length, this->on_read, this->read_counter[fd]);

	ssize_t len;
	uint32_t block_size;
	std::string recv_data;
	while(true){
		if((len = read(fd, data, 4)) < 0){
			if(errno != EWOULDBLOCK){
				perror("encryptor read");
				ERROR("encryptor read " << fd)
				return -1;
			}
			break;
		}else if(len == 0){
			ERROR("encryptor read zero")
			return -2;
		}else if(len != 4){
			ERROR("couldn't read block size?")
			return -3;
		}
		block_size = Util::read_uint32_t(data);

		if((len = read(fd, data, static_cast<size_t>(block_size))) < 0){
			if(errno != EWOULDBLOCK){
				perror("encryptor read block");
				ERROR("encryptor read block " << fd)
				return -1;
			}
			return 0;
		}else if(len == 0){
			ERROR("encryptor read zero block")
			return -2;
		}else if(len != block_size){
			ERROR("couldn't read block size block?")
			return -3;
		}

		data[len] = 0;
		PRINT("RDATA|" << data << '|')
		try{
			recv_data = this->decrypt(std::string(data), transaction);
		}catch(const std::exception& e){
			ERROR(e.what() << "\nImplied hacker, closing!")
			return -4;
		}
		if(callback(fd, recv_data.c_str(), recv_data.length()) < 0){
			return -1;
		}
	}

	return len;
}
