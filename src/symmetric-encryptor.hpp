#pragma once

#include "cryptopp/aes.h"
#include "cryptopp/osrng.h"
#include "cryptopp/modes.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

class SymmetricEncryptor{
private:
	CryptoPP::AutoSeededRandomPool random_pool;
	byte key[CryptoPP::AES::MAX_KEYLENGTH];
	byte iv[CryptoPP::AES::BLOCKSIZE];
	byte salt[CryptoPP::AES::MAX_KEYLENGTH];

public:
	SymmetricEncryptor(std::string keyfile);

	std::string encrypt(std::string data);
	std::string decrypt(std::string data);
};
