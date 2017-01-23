#pragma once

#include "cryptopp/aes.h"
#include "cryptopp/osrng.h"
#include "cryptopp/modes.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "tcp-server.hpp"
#include "symmetric-encryptor.hpp"

class SymmetricEpollServer : public EpollServer {
private:
	SymmetricEncryptor encryptor;
public:
	SymmetricEpollServer(std::string keyfile, uint16_t port, size_t new_max_connections);

	bool send(int fd, const char* data, size_t data_length);
	ssize_t recv(int fd, char* data, size_t data_length);
};
