#include <sys/types.h>
#include <fcntl.h>

#include "util.hpp"
#include "symmetric-event-server.hpp"

SymmetricEventServer::SymmetricEventServer(std::string keyfile, uint16_t port, size_t new_max_connections)
:EventServer("SymmetricEventServer", port, new_max_connections),
encryptor(keyfile){}

bool SymmetricEventServer::send(int fd, const char* data, size_t /*data_length*/){
	// Might fix encryption buggies...
	//std::string send_data = this->encrypt(data, data_length);
	std::string send_data = this->encryptor.encrypt(data);
	if(write(fd, send_data.c_str(), send_data.length()) < 0){
		return true;
	}
	return false;
}

bool SymmetricEventServer::recv(int fd, char* data, size_t data_length){
	ssize_t len;
	std::string recv_data;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK){
			ERROR(this->name << " read")
			return true;
		}
		return false;
	}else if(len == 0){
		PRINT(this->name << ' ' << fd << " read zero, closing")
		return true;
	}else{
		data[len] = 0;
		try{
			// Might fix decryption buggies...
			//recv_data = this->decrypt(std::string(data, len));
			recv_data = this->encryptor.decrypt(std::string(data));
		}catch(const CryptoPP::Exception& e){
			PRINT(e.what() << "\nImplied hacker, closing!")
			return true;
		}
		return packet_received(fd, recv_data.c_str(), static_cast<ssize_t>(recv_data.length()));
	}
}
