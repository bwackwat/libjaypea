#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>

#include "cryptopp/aes.h"
#include "cryptopp/cryptlib.h"
#include "cryptopp/osrng.h"
#include "cryptopp/hex.h"

int main(int, char**){
	byte key[CryptoPP::AES::MAX_KEYLENGTH];
	byte iv[CryptoPP::AES::BLOCKSIZE];
	CryptoPP::AutoSeededRandomPool rand_tool;
	rand_tool.GenerateBlock(key, CryptoPP::AES::MAX_KEYLENGTH);
	rand_tool.GenerateBlock(iv, CryptoPP::AES::BLOCKSIZE);
	std::cout << "Creating \"artifacts/keyfile.new\" file." << std::endl;
	int keyfile = open("artifacts/keyfile.new", O_WRONLY | O_CREAT | O_TRUNC);
	std::string keystr;
	CryptoPP::HexEncoder hex(new CryptoPP::StringSink(keystr));
	hex.Put(key, CryptoPP::AES::MAX_KEYLENGTH);
	hex.Put(iv, CryptoPP::AES::BLOCKSIZE);
	hex.MessageEnd();
	write(keyfile, keystr.c_str(), keystr.length());
	close(keyfile);
	return 0;
}
