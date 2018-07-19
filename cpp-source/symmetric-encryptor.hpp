#pragma once

#include "cryptopp/aes.h"
#include "cryptopp/osrng.h"
#include "cryptopp/hmac.h"
#include "cryptopp/modes.h"
#include "cryptopp/base64.h"
#include "cryptopp/hex.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

union Size{
	size_t size;
	char chars[4];
};

class SymmetricEncryptor{
private:
	CryptoPP::AutoSeededRandomPool random_pool;
	byte key[CryptoPP::AES::MAX_KEYLENGTH];
	byte iv[CryptoPP::AES::BLOCKSIZE];
	byte hmac_key[CryptoPP::AES::MAX_KEYLENGTH];
	CryptoPP::HMAC<CryptoPP::SHA256> hmac;
public:
	SymmetricEncryptor(std::string keyfile = std::string());

	std::string encrypt(std::string data);
	std::string decrypt(std::string data);
	std::string encrypt(std::string data, int transaction);
	std::string decrypt(std::string data, int transaction);
	
	bool send(int fd, const char* data, size_t data_length, int* transaction);
	ssize_t recv(int fd, char* data, size_t data_length,
	std::function<ssize_t(int, const char*, ssize_t)> callback, int* transaction);
};
