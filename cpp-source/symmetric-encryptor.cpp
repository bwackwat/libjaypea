#include <fcntl.h>

#include "util.hpp"
#include "symmetric-encryptor.hpp"

SymmetricEncryptor::SymmetricEncryptor(){
	this->random_pool.GenerateBlock(this->key, CryptoPP::AES::MAX_KEYLENGTH);
	this->random_pool.GenerateBlock(this->iv, CryptoPP::AES::BLOCKSIZE);
}

std::string SymmetricEncryptor::get_sha256_hash(std::string data){
        std::string hash;
        CryptoPP::SHA256 hasher;

        CryptoPP::StringSource source(data, true,
                new CryptoPP::HashFilter(hasher,
                new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(hash))));

        return hash;
}

std::string SymmetricEncryptor::encrypt(std::string data){
	std::string new_data;
	CryptoPP::StringSink* sink = new CryptoPP::StringSink(new_data);
	CryptoPP::Base64Encoder* base64_enc = new CryptoPP::Base64Encoder(sink, false);
	CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption enc(this->key, CryptoPP::AES::MAX_KEYLENGTH, this->iv);
	CryptoPP::StreamTransformationFilter* aes_enc = new CryptoPP::StreamTransformationFilter(enc, base64_enc);
	CryptoPP::StringSource enc_source(data, true, aes_enc);

	std::string hashed_cyphertext = this->get_sha256_hash(new_data + std::string(reinterpret_cast<const char*>(this->key), CryptoPP::AES::MAX_KEYLENGTH));
	// SHA256 produces 64 BASE32 bytes, BASE64 45 bytes.
	if(hashed_cyphertext.length() != 45){
		PRINT("SHA256 in BASE64 length not 45 bytes?" << hashed_cyphertext.length())
	}
	return hashed_cyphertext + new_data;
}

std::string SymmetricEncryptor::decrypt(std::string data){
	std::string cyphertext = data.substr(45);
	std::string hash = data.substr(0, 45);
	if(hash != this->get_sha256_hash(cyphertext + std::string(reinterpret_cast<const char*>(this->key), CryptoPP::AES::MAX_KEYLENGTH))){
		throw std::runtime_error("Bad hash, yikes!");
	}

	std::string new_data;
	CryptoPP::StringSink* sink = new CryptoPP::StringSink(new_data);
	CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption dec(this->key, CryptoPP::AES::MAX_KEYLENGTH, this->iv);
	CryptoPP::StreamTransformationFilter* aes_dec = new CryptoPP::StreamTransformationFilter(dec, sink);
	CryptoPP::Base64Decoder* base64_dec = new CryptoPP::Base64Decoder(aes_dec);
	CryptoPP::StringSource dec_source(cyphertext, true, base64_dec);
	return new_data;
}

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

std::string SymmetricEncryptor::encrypt(std::string data, int transaction){
	byte salt[CryptoPP::AES::BLOCKSIZE];
	this->random_pool.GenerateBlock(salt, CryptoPP::AES::BLOCKSIZE);
	std::string salts(reinterpret_cast<const char*>(salt), CryptoPP::AES::BLOCKSIZE);

	DEBUG("STRANS:" << static_cast<unsigned char>(transaction % 255))
	DEBUG("STRANS:" << static_cast<char>(transaction % 255))

	return this->encrypt(static_cast<char>(transaction % 255) + salts + data);
}

std::string SymmetricEncryptor::decrypt(std::string data, int transaction){
	std::string new_data = this->decrypt(data);

	DEBUG("RTRANS:" << static_cast<unsigned char>(new_data[0]))
	DEBUG("RTRANS:" << static_cast<char>(new_data[0]))
	if(static_cast<unsigned char>(new_data[0]) != static_cast<unsigned char>(transaction % 255)){
		PRINT("got " << static_cast<int>(new_data[0]) << " expected " << transaction)
		throw CryptoPP::Exception(CryptoPP::BERDecodeErr::INVALID_DATA_FORMAT, "Bad transaction number.");
	}
	new_data.erase(0, CryptoPP::AES::BLOCKSIZE + 1);
	return new_data;
}

bool SymmetricEncryptor::send(int fd, const char* data, size_t /* data_length */, int* transaction){
	std::string send_data = this->encrypt(data, *transaction);
	*transaction += 1;
	
	char block_size[4] = {0, 0, 0, 0};
	*(reinterpret_cast<uint32_t*>(block_size)) = static_cast<uint32_t>(send_data.length());
	
	//Util::write_size_t(send_data.length(), block_size);
	ssize_t len;

	DEBUG("SDATA|" << send_data.length() << '|' <<  block_size)
	if((len = write(fd, block_size, 4)) < 0){
		ERROR("encryptor write")
		return true;
	}else if(len != 4){
		ERROR("encryptor didn't write all")
		return true;
	}

	DEBUG("SDATA|" << send_data << '|')
	if((len = write(fd, send_data.c_str(), send_data.length())) < 0){
		ERROR("encryptor write block")
		return true;
	}else if(len != static_cast<ssize_t>(send_data.length())){
		ERROR("encryptor didn't write all block")
		return true;
	}
	return false;
}

ssize_t SymmetricEncryptor::recv(int fd, char* data, size_t data_length,
std::function<ssize_t(int, const char*, ssize_t)> callback, int* transaction){
	ssize_t len;
	size_t block_size;
	std::string recv_data;
	char size_data[4] = {0, 0, 0, 0};
	
	if((len = read(fd, size_data, 4)) < 0){
		if(errno != EWOULDBLOCK){
			perror("encryptor read");
			ERROR("encryptor read " << fd)
			return -1;
		}
		return 0;
	}else if(len == 0){
		ERROR("encryptor read zero")
		return -2;
	}else if(len != 4){
		ERROR("couldn't read block size?" << len)
		return -3;
	}
	block_size = *(reinterpret_cast<uint32_t*>(size_data));
	//block_size = Util::read_size_t(data);
	DEBUG("RDATA|" << size_data << '|' << block_size)
	if(block_size >= data_length){
		PRINT("Received too large of a packet!")
		return -1;
	}

	while(true){
		if((len = read(fd, data, block_size)) < 0){
			if(errno != EWOULDBLOCK){
				perror("encryptor read block");
				ERROR("encryptor read block " << fd << '|' << block_size)
				return -1;
			}
			continue;
		}else if(len == 0){
			ERROR("encryptor read zero block")
			return -2;
		}else if(len != static_cast<ssize_t>(block_size)){
			ERROR("couldn't read block size block?" << len << '|' << block_size)
			return -3;
		}

		data[len] = 0;
		DEBUG("RDATA|" << data << '|')
		try{
			recv_data = this->decrypt(std::string(data), *transaction);
			*transaction += 1;
		}catch(const std::exception& e){
			ERROR(e.what() << "\nImplied hacker, closing!")
			return -4;
		}
		return callback(fd, recv_data.c_str(), static_cast<ssize_t>(recv_data.length()));
	}
}

/*
bool SymmetricEncryptor::send(int fd, const char* data, size_t, int transaction){
	std::string send_data = this->encrypt(data, transaction);
	ssize_t len;
	PRINT("SDATA|" << send_data << '|')
	if((len = write(fd, send_data.c_str(), send_data.length())) < 0){
		ERROR("encryptor write")
		return true;
	}else if(len != static_cast<ssize_t>(send_data.length())){
		ERROR("encryptor didn't write all")
		return true;
	}
	return false;
}*/

/**
 * 1. Reads uint32_t for the size of the encrypted block.
 * 2. Reads that size for the encrypted block.
 * 3. Decrypts the block.
 * 4. Starts back at 1, and tries to read another uint32_t in case many messages merged.
 * 
 * Are TCP messages merging a consequence of EPOLLONESHOT? Even if they are, I still prefer it.
 */
/*ssize_t SymmetricEncryptor::recv(int fd, char* data, size_t data_length,
std::function<ssize_t(int, const char*, size_t)> callback, int transaction){
	ssize_t len;
	std::string recv_data;
	if((len = read(fd, data, data_length)) < 0){
		if(errno != EWOULDBLOCK){
			perror("encryptor read");
			ERROR("encryptor read " << fd)
			return -1;
		}
		return 0;
	}else if(len == 0){
		ERROR("encryptor read zero")
		return -2;
	}else{
		data[len] = 0;
		PRINT("RDATA|" << data << '|')
		try{
			recv_data = this->decrypt(std::string(data), transaction);
		}catch(const std::exception& e){
			ERROR(e.what() << "\nImplied hacker, closing!")
			return -3;
		}
		if(callback != nullptr){
			return callback(fd, recv_data.c_str(), recv_data.length());
		}
		std::strcpy(data, recv_data.c_str());
		return static_cast<ssize_t>(recv_data.length());
	}
}*/
