#include <fcntl.h>

#include "util.hpp"
#include "symmetric-encryptor.hpp"

SymmetricEncryptor::SymmetricEncryptor(std::string keyfile){
	if(keyfile.empty()){
		this->random_pool.GenerateBlock(this->key, CryptoPP::AES::MAX_KEYLENGTH);
		this->random_pool.GenerateBlock(this->iv, CryptoPP::AES::BLOCKSIZE);
		this->random_pool.GenerateBlock(this->hmac_key, CryptoPP::AES::MAX_KEYLENGTH);
		
		this->hmac = CryptoPP::HMAC<CryptoPP::SHA256>(hmac_key, CryptoPP::AES::MAX_KEYLENGTH);
		
		/*
		std::string keystr;
		CryptoPP::HexEncoder hex(new CryptoPP::StringSink(keystr));
		hex.Put(this->key, CryptoPP::AES::MAX_KEYLENGTH);
		hex.Put(this->iv, CryptoPP::AES::BLOCKSIZE);
		hex.MessageEnd();
		*/
		PRINT("SymmetricEncryptor generated a key.")
		return;
	}

	const size_t hex_key_length = (CryptoPP::AES::MAX_KEYLENGTH + CryptoPP::AES::BLOCKSIZE) * 2; // 96
	byte hex_key[hex_key_length + 1];
	int fd;
	if((fd = open(keyfile.c_str(), O_RDONLY)) < 0){
		throw std::runtime_error("SymmetricEncryptor open keyfile");
	}
	if(read(fd, hex_key, hex_key_length) < static_cast<ssize_t>(hex_key_length)){
		throw std::runtime_error("SymmetricEncryptor read keyfile");
	}
	if(close(fd) < 0){
		throw std::runtime_error("SymmetricEncryptor close keyfile");
	}
	hex_key[hex_key_length] = 0;
	
	PRINT("PROVIDED KEY: " << hex_key)
	
	CryptoPP::ArraySource key_source(hex_key, CryptoPP::AES::MAX_KEYLENGTH * 2, true,
		new CryptoPP::HexDecoder(
		new CryptoPP::ArraySink(this->key, CryptoPP::AES::MAX_KEYLENGTH)));
	
	CryptoPP::ArraySource iv_source(hex_key + CryptoPP::AES::MAX_KEYLENGTH * 2, CryptoPP::AES::BLOCKSIZE * 2, true,
		new CryptoPP::HexDecoder(
		new CryptoPP::ArraySink(this->iv, CryptoPP::AES::BLOCKSIZE)));
	
	/*
	
	// TEST CODE
	
	std::string keystr;
	CryptoPP::HexEncoder hex(new CryptoPP::StringSink(keystr));
	hex.Put(this->key, CryptoPP::AES::MAX_KEYLENGTH);
	hex.Put(this->iv, CryptoPP::AES::BLOCKSIZE);
	hex.MessageEnd();
	
	PRINT("PROVIDED KEY: " << this->key << this->iv)
	PRINT("PROVIDED KEY: " << keystr)
	
	if(std::strncmp(reinterpret_cast<char*>(hex_key), keystr.c_str(), 96) != 0){
		throw std::runtime_error("Bad hex decoding.");
	}
	
	*/
	
	this->hmac = CryptoPP::HMAC<CryptoPP::SHA256>(this->key, CryptoPP::AES::MAX_KEYLENGTH);

	// Another random number tool.
	// srand(static_cast<unsigned int>(time(0)));
}

std::string SymmetricEncryptor::encrypt(std::string data){
	CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption encryptor(this->key, CryptoPP::AES::MAX_KEYLENGTH, this->iv);
	
	//byte salt[CryptoPP::AES::BLOCKSIZE];
	//this->random_pool.GenerateBlock(salt, CryptoPP::AES::BLOCKSIZE);
	//std::string salts(reinterpret_cast<const char*>(salt), CryptoPP::AES::BLOCKSIZE);

	std::string encrypted_data;
	//CryptoPP::StringSource pipeline1(salts + data, true,
	CryptoPP::StringSource pipeline1(data, true,
		new CryptoPP::StreamTransformationFilter(encryptor,
		new CryptoPP::StringSink(encrypted_data)));

	std::string hash;
	CryptoPP::StringSource pipeline2(encrypted_data, true,
		new CryptoPP::HashFilter(this->hmac,
		new CryptoPP::StringSink(hash)));

	// HMAC SHA256 produces 32 bytes.
	if(hash.length() != 32){
		PRINT("SHA256 in BASE64 length not 32 bytes?" << hash.length())
	}

	std::string base64_encrypted_data;
	CryptoPP::StringSource pipeline3(hash + encrypted_data, true,
		new CryptoPP::Base64Encoder(
		new CryptoPP::StringSink(base64_encrypted_data)));
	return base64_encrypted_data;
}

std::string SymmetricEncryptor::decrypt(std::string data){
	CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption decryptor(this->key, CryptoPP::AES::MAX_KEYLENGTH, this->iv);

	std::string encrypted_data;
	CryptoPP::StringSource pipeline1(data, true,
		new CryptoPP::Base64Decoder(
		new CryptoPP::StringSink(encrypted_data)));

	if(encrypted_data.length() % 32 != 0 || encrypted_data.length() == 0){
		//throw std::runtime_error("Bad encrpyted data size (" + std::to_string(encrypted_data.length()) + ").");
	}

	CryptoPP::StringSource pipeline2(encrypted_data, true,
		new CryptoPP::HashVerificationFilter(this->hmac, nullptr,
		CryptoPP::HashVerificationFilter::THROW_EXCEPTION |
		CryptoPP::HashVerificationFilter::HASH_AT_BEGIN));

	std::string decrypted_data;
	CryptoPP::StringSource pipeline3(encrypted_data.substr(32), true,
		new CryptoPP::StreamTransformationFilter(decryptor,
		new CryptoPP::StringSink(decrypted_data)));

	//decrypted_data.erase(0, CryptoPP::AES::BLOCKSIZE);
	return decrypted_data;
}

std::string SymmetricEncryptor::encrypt(std::string data, int transaction){
	DEBUG("STRANS:" << static_cast<unsigned char>(transaction % 255))
	DEBUG("STRANS:" << static_cast<char>(transaction % 255))
	DEBUG("STRANS:" << static_cast<int>(transaction % 255))
	DEBUG("STEXT:" << std::string(static_cast<char>(transaction % 255) + data))

	return this->encrypt(static_cast<char>(transaction % 255) + data);
	//return this->encrypt(data);
}

std::string SymmetricEncryptor::decrypt(std::string data, int transaction){
	std::string new_data = this->decrypt(data);

	DEBUG("RTRANS:" << static_cast<unsigned char>(new_data[0]))
	DEBUG("RTRANS:" << static_cast<char>(new_data[0]))
	DEBUG("RTRANS:" << static_cast<int>(new_data[0]))
	DEBUG("RTEXT:" << new_data)
	
	//TODO This transaction business was causing incorrect HMAC errors.
	
	if(static_cast<unsigned char>(new_data[0]) != static_cast<unsigned char>(transaction % 255)){
		PRINT("got " << static_cast<int>(new_data[0]) << " expected " << transaction)
		throw CryptoPP::Exception(CryptoPP::BERDecodeErr::INVALID_DATA_FORMAT, "Bad transaction number.");
	}
	new_data.erase(0, 1);
	return new_data;
}

bool SymmetricEncryptor::send(int fd, const char* data, size_t /* data_length */, int* transaction){
	ssize_t len;
	std::string send_data = this->encrypt(data, *transaction);

	char block_size[4] = {0, 0, 0, 0};
	*(reinterpret_cast<uint32_t*>(block_size)) = static_cast<uint32_t>(send_data.length());

	// Should be consistently 89 bytes.
	std::string send_data_size = this->encrypt(std::string(block_size, 4), *transaction);
	DEBUG("SIZE DATA LENGTH: " << send_data_size.length())

	*transaction += 1;

	DEBUG("SDATA|" << send_data_size << '|' << send_data_size.length())
	if((len = write(fd, send_data_size.c_str(), 89)) < 0){
		ERROR("encryptor write")
		return true;
	}else if(len != 89){
		ERROR("encryptor didn't write all")
		return true;
	}

	DEBUG("SDATA|" << send_data << '|' << send_data.length())
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
	std::string recv_data;
	std::string recv_size_data;
	
	if((len = read(fd, data, 89)) < 0){
		if(errno != EWOULDBLOCK){
			perror("encryptor read");
			ERROR("encryptor read " << fd)
			return -1;
		}
		return 0;
	}else if(len == 0){
		ERROR("encryptor read zero")
		return -2;
	}else if(len != 89){
		ERROR("couldn't read block size?" << len)
		return -3;
	}
	data[len] = 0;
	try{
		recv_size_data = this->decrypt(std::string(data, static_cast<size_t>(len)), *transaction);
	}catch(const std::exception& e){
		ERROR(e.what() << "\nImplied hacker, closing!")
		return -4;
	}
	if(recv_size_data.length() != 4){
		PRINT("Did not receive 4 bytes or decrypted length...")
		return -5;
	}
	
	const unsigned long block_size = *(reinterpret_cast<const uint32_t*>(recv_size_data.c_str()));
	//block_size = Util::read_size_t(data);
	DEBUG("RDATA|" << data << '|' << block_size)
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
			PRINT("Server kicked you.")
			return -2;
		}else if(len != static_cast<ssize_t>(block_size)){
			ERROR("couldn't read block size block?" << len << '|' << block_size)
			return -3;
		}

		data[len] = 0;
		DEBUG("RDATA|" << data << '|')
		try{
			recv_data = this->decrypt(std::string(data, static_cast<size_t>(len)), *transaction);
			*transaction += 1;
		}catch(const std::exception& e){
			ERROR(e.what() << "\nImplied hacker, closing!")
			return -4;
		}
		return callback(fd, recv_data.c_str(), static_cast<ssize_t>(recv_data.length()));
	}
}
