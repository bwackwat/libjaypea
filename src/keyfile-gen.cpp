#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>

#include "cryptopp/aes.h"
#include "cryptopp/cryptlib.h"
#include "cryptopp/osrng.h"

int main(int argc, char** argv){
	byte key[CryptoPP::AES::MAX_KEYLENGTH];
	byte iv[CryptoPP::AES::BLOCKSIZE];
	CryptoPP::AutoSeededRandomPool rand_tool;
	int keyfile = open("etc/keyfile.new", O_WRONLY | O_CREAT | O_TRUNC);

	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "-h") == 0 ||
		std::strcmp(argv[i], "--help") == 0){
			std::cout << "Creates the \"etc/keyfile.new\" file." << std::endl;
		}
	}

	rand_tool.GenerateBlock(key, CryptoPP::AES::MAX_KEYLENGTH);
	rand_tool.GenerateBlock(iv, CryptoPP::AES::BLOCKSIZE);
	write(keyfile, key, CryptoPP::AES::MAX_KEYLENGTH);
	write(keyfile, iv, CryptoPP::AES::BLOCKSIZE);
	close(keyfile);
	return 0;
}
