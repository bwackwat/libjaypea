#include "symmetric-event-client.hpp"

SymmetricEventClient::SymmetricEventClient(std::string keyfile)
:encryptor(keyfile){}

bool SymmetricEventClient::send(int fd, const char* data, size_t /*data_length*/){
	// Might fix encryption buggies...
	//std::string send_data = this->encrypt(data, data_length);
	std::string send_data = this->encryptor.encrypt(data);
	if(write(fd, send_data.c_str(), send_data.length()) < 0){
		return true;
	}
	return false;
}

bool SymmetricEventClient::recv(int fd, char* data, size_t data_length){
	ssize_t len;
	std::string recv_data;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK){
			ERROR("read")
			return true;
		}
		return false;
	}else if(len == 0){
		ERROR("client read zero")
		return true;
	}else{
		data[len] = 0;
		try{
			// Might fix decryption buggies...
			//recv_data = this->decrypt(std::string(data, len));
			recv_data = this->encryptor.decrypt(std::string(data));
		}catch(const CryptoPP::Exception& e){
			ERROR(e.what() << "\nImplied hacker, closing!")
			return true;
		}
		return this->on_read(fd, recv_data.c_str(), static_cast<ssize_t>(recv_data.length()));
	}
}
