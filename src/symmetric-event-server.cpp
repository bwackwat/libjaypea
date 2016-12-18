#include <sys/types.h>
#include <fcntl.h>

#include "util.hpp"
#include "symmetric-event-server.hpp"

SymmetricEventServer::SymmetricEventServer(std::string keyfile, uint16_t port, size_t new_max_connections) : EventServer("SymmetricEventServer", port, new_max_connections){
	int fd;
	if((fd = open(keyfile.c_str(), O_RDONLY)) < 0){
		throw std::runtime_error(this->name + " open keyfile");
	}
	if(read(fd, this->key, CryptoPP::AES::MAX_KEYLENGTH) < CryptoPP::AES::MAX_KEYLENGTH){
		throw std::runtime_error(this->name + " read keyfile key");
	}
	if(read(fd, this->iv, CryptoPP::AES::BLOCKSIZE) < CryptoPP::AES::BLOCKSIZE){
		throw std::runtime_error(this->name + " read keyfile iv");
	}
	if(close(fd) < 0){
		throw std::runtime_error(this->name + " close keyfile");
	}
	//srand(static_cast<unsigned int>(time(0)));
}

std::string SymmetricEventServer::encrypt(std::string data){
	DEBUG("ENCRYPT:\n" << data << '|' << data.length())
	this->random_pool.GenerateBlock(this->salt, CryptoPP::AES::MAX_KEYLENGTH);
	std::string salted(reinterpret_cast<const char*>(this->salt), CryptoPP::AES::MAX_KEYLENGTH);
	DEBUG("SALT:\n" << salted << '|' << salted.length());
	salted += data;
	std::string new_data;

	CryptoPP::StringSink* sink = new CryptoPP::StringSink(new_data);
	CryptoPP::Base64Encoder* base64_enc = new CryptoPP::Base64Encoder(sink, false);
	CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption enc(this->key, CryptoPP::AES::MAX_KEYLENGTH, this->iv);
	CryptoPP::StreamTransformationFilter* aes_enc = new CryptoPP::StreamTransformationFilter(enc, base64_enc);
	CryptoPP::StringSource enc_source(salted, true, aes_enc);

	DEBUG("ENCRYPTED:\n" << new_data << '|' << new_data.length());
	return new_data;
}

std::string SymmetricEventServer::decrypt(std::string data){
	DEBUG("DECRYPT:\n" << data << '|' << data.length())
	std::string new_data;

	CryptoPP::StringSink* sink = new CryptoPP::StringSink(new_data);
	CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption dec(this->key, CryptoPP::AES::MAX_KEYLENGTH, this->iv);
	CryptoPP::StreamTransformationFilter* aes_dec = new CryptoPP::StreamTransformationFilter(dec, sink);
	CryptoPP::Base64Decoder* base64_dec = new CryptoPP::Base64Decoder(aes_dec);
	CryptoPP::StringSource dec_source(data, true, base64_dec);

	DEBUG("DECRYPTED:\n" << new_data << '|'  << new_data.length());
	new_data.erase(0, CryptoPP::AES::MAX_KEYLENGTH);
	DEBUG("SALTFREE:\n" << new_data << '|'  << new_data.length());
	return new_data;
}

bool SymmetricEventServer::send(int fd, const char* data, size_t /*data_length*/){
	// Might fix encryption buggies...
	//std::string send_data = this->encrypt(data, data_length);
	std::string send_data = this->encrypt(data);
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
			recv_data = this->decrypt(std::string(data));
		}catch(const CryptoPP::Exception& e){
			PRINT(e.what() << "\nImplied hacker, closing!")
			return true;
		}
		return packet_received(fd, recv_data.c_str(), static_cast<ssize_t>(recv_data.length()));
	}
}
