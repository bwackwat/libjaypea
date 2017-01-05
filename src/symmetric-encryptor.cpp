#include <fcntl.h>

#include "util.hpp"
#include "symmetric-encryptor.hpp"

SymmetricEncryptor::SymmetricEncryptor(std::string keyfile){
	std::string name = "SymmetricEncryptor";
	int fd;
	if((fd = open(keyfile.c_str(), O_RDONLY)) < 0){
		throw std::runtime_error(name + " open keyfile");
	}
	if(read(fd, this->key, CryptoPP::AES::MAX_KEYLENGTH) < CryptoPP::AES::MAX_KEYLENGTH){
		throw std::runtime_error(name + " read keyfile key");
	}
	if(read(fd, this->iv, CryptoPP::AES::BLOCKSIZE) < CryptoPP::AES::BLOCKSIZE){
		throw std::runtime_error(name + " read keyfile iv");
	}
	if(close(fd) < 0){
		throw std::runtime_error(name + " close keyfile");
	}
	//srand(static_cast<unsigned int>(time(0)));
}

std::string SymmetricEncryptor::encrypt(std::string data){
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

std::string SymmetricEncryptor::decrypt(std::string data){
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

bool SymmetricEncryptor::send(int fd, const char* data, size_t /*data_length*/){
	// Might fix encryption buggies...
	//std::string send_data = this->encrypt(data, data_length);
	std::string send_data = this->encrypt(data);
	if(write(fd, send_data.c_str(), send_data.length()) < 0){
		ERROR("encryptor write")
		return true;
	}
	return false;
}

bool SymmetricEncryptor::recv(int fd, char* data, size_t data_length,
std::function<bool(int, const char*, size_t)> callback){
	ssize_t len;
	std::string recv_data;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK){
			ERROR("encryptor read")
			return true;
		}
		return false;
	}else if(len == 0){
		ERROR("encryptor read zero")
		return true;
	}else{
		data[len] = 0;
		try{
			// Might fix decryption buggies...
			//recv_data = this->decrypt(std::string(data, len));
			recv_data = this->decrypt(std::string(data));
		}catch(const CryptoPP::Exception& e){
			ERROR(e.what() << "\nImplied hacker, closing!")
			return true;
		}
		return callback(fd, recv_data.c_str(), recv_data.length());
	}
}
