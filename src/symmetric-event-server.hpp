#pragma once

#include "cryptopp/aes.h"
#include "cryptopp/osrng.h"
#include "cryptopp/modes.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "tcp-event-server.hpp"

class SymmetricEventServer : public EventServer {
private:
	CryptoPP::AutoSeededRandomPool random_pool;
	byte key[CryptoPP::AES::MAX_KEYLENGTH];
	byte iv[CryptoPP::AES::BLOCKSIZE];
	byte salt[CryptoPP::AES::MAX_KEYLENGTH];

	std::string encrypt(std::string data);
	std::string decrypt(std::string data);
public:
	SymmetricEventServer(std::string keyfile, uint16_t port, int new_max_connections);

	bool send(int fd, char* data, size_t data_length);
	bool recv(int fd, char* data, size_t data_length);
};
