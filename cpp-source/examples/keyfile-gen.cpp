#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>

#include "cryptopp/aes.h"
#include "cryptopp/cryptlib.h"
#include "cryptopp/osrng.h"

int main(int, char**){
	byte key[CryptoPP::AES::MAX_KEYLENGTH];
	byte iv[CryptoPP::AES::BLOCKSIZE];
	CryptoPP::AutoSeededRandomPool rand_tool;
	rand_tool.GenerateBlock(key, CryptoPP::AES::MAX_KEYLENGTH);
	rand_tool.GenerateBlock(iv, CryptoPP::AES::BLOCKSIZE);
	std::cout << "Creating \"extras/keyfile.new\" file." << std::endl;
	int keyfile = open("extras/keyfile.new", O_WRONLY | O_CREAT | O_TRUNC);
	write(keyfile, key, CryptoPP::AES::MAX_KEYLENGTH);
	write(keyfile, iv, CryptoPP::AES::BLOCKSIZE);
	close(keyfile);
	return 0;
}
