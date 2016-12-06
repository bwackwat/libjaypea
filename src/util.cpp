#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <thread>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cryptopp/aes.h"
#include "cryptopp/osrng.h"
#include "cryptopp/modes.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "util.hpp"

std::string IDENTITY = "Hello this is my identity it is default.";
std::string VERIFIED = "Your connection has been verified.";

std::map<enum Routine, std::string> ROUTINES = {
	{ SHELL, "I would like to use the shell please." },
	{ SEND_FILE, "I would like to send a file please." },
	{ RECV_FILE, "I would like to receive a file please." }
};

std::string START_ROUTINE = "You are welcome to start the requested routine.";
std::string BAD_ROUTINE = "You have somehow requested an invalid routine!";

// 32 bytes, 256 bits.
byte* KEY = new byte[CryptoPP::AES::MAX_KEYLENGTH];
// 16 bytes, 128 bits.
byte* IV = new byte[CryptoPP::AES::BLOCKSIZE];
byte* SALT = new byte[CryptoPP::AES::MAX_KEYLENGTH];
CryptoPP::AutoSeededRandomPool rand_tool;

int init_crypto(std::string keyfile){
	int file = open(keyfile.c_str(), O_RDONLY);
	if(read(file, KEY, CryptoPP::AES::MAX_KEYLENGTH) < CryptoPP::AES::MAX_KEYLENGTH){
		std::cout << "Couldn't read key from keyfile.\n";
	}
	if(read(file, IV, CryptoPP::AES::BLOCKSIZE) < CryptoPP::AES::BLOCKSIZE){
		std::cout << "Couldn't read iv from keyfile.\n";
	}

	srand(static_cast<unsigned int>(time(0)));

	std::cout << "KEY:" << KEY << std::endl;
	std::cout << "IV:" << IV << std::endl;

	return 0;
}

int send(int fd, std::string data){
	std::string send_data = encrypt(data);
	if(write(fd, send_data.c_str(), send_data.length()) < 0){
		return 1;
	}
	return 0;
}

void set_non_blocking(int fd){
	int flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
}

std::string encrypt(std::string data){
	DEBUG("ENCRYPT:\n" << data << '|' << data.length())
	rand_tool.GenerateBlock(SALT, CryptoPP::AES::MAX_KEYLENGTH);
	std::string salted(reinterpret_cast<const char*>(SALT), CryptoPP::AES::MAX_KEYLENGTH);
	DEBUG("SALT:\n" << salted << '|' << salted.length());
	salted += data;
	std::string new_data;

	CryptoPP::StringSink* sink = new CryptoPP::StringSink(new_data);
	CryptoPP::Base64Encoder* base64_enc = new CryptoPP::Base64Encoder(sink, false);
	CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption enc(KEY, CryptoPP::AES::MAX_KEYLENGTH, IV);
	CryptoPP::StreamTransformationFilter* aes_enc = new CryptoPP::StreamTransformationFilter(enc, base64_enc);
	CryptoPP::StringSource enc_source(salted, true, aes_enc);

	DEBUG("ENCRYPTED:\n" << new_data << '|' << new_data.length());
	return new_data;
}

std::string decrypt(std::string data){
	DEBUG("DECRYPT:\n" << data << '|' << data.length())
	std::string new_data;

	CryptoPP::StringSink* sink = new CryptoPP::StringSink(new_data);
	CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption dec(KEY, CryptoPP::AES::MAX_KEYLENGTH, IV);
	CryptoPP::StreamTransformationFilter* aes_dec = new CryptoPP::StreamTransformationFilter(dec, sink);
	CryptoPP::Base64Decoder* base64_dec = new CryptoPP::Base64Decoder(aes_dec);
	CryptoPP::StringSource dec_source(data, true, base64_dec);

	DEBUG("DECRYPTED:\n" << new_data << '|'  << new_data.length());
	new_data.erase(0, CryptoPP::AES::MAX_KEYLENGTH);
	DEBUG("SALTFREE:\n" << new_data << '|'  << new_data.length());
	return new_data;
}

int send_file_routine(int socketfd, std::string file_path){
	int filefd;
	ssize_t len;
	char packet[PACKET_LIMIT];
	char file_part[FILE_PART_LIMIT];
	std::string verify_part;
	struct stat file_stat;

	if(stat(file_path.c_str(), &file_stat) == 0){
		if(!(file_stat.st_mode & S_IFREG)){
			std::cout << "You provided something other than a plain file." << std::endl;
			return 1;
		}
	}else{
		std::cout << "Uh oh, stat error!" << std::endl;
		return 1;
	}

	if((filefd = open(file_path.c_str(), O_RDONLY)) < 0){
		std::cout << "Uh oh, open error!" << std::endl;
		return 1;
	}

	if(send(socketfd, file_path)){
		std::cout << "Uh oh, send file error!" << std::endl;
		return 1;
	}

	if((len = read(socketfd, packet, PACKET_LIMIT)) < 0){
		std::cout << "Uh oh, read method error." << std::endl;
		return 1;
	}
	packet[len] = 0;
	verify_part = decrypt(packet);
	std::cout << verify_part << std::endl;

	int part = 0;
	ssize_t rlen;
	while(1){
		if((rlen = read(filefd, file_part, FILE_PART_LIMIT)) < 0){
			std::cout << "Uh oh, read file error!" << std::endl;
			break;
		}
		file_part[rlen] = 0;
		if(send(socketfd, file_part)){
			std::cout << "Uh oh, send file part error!" << part << std::endl;
			break;
		}
		if((len = read(socketfd, packet, PACKET_LIMIT)) < 0){
			std::cout << "Uh oh, read part verify error." << std::endl;
			break;
		}
		packet[len] = 0;
		verify_part = decrypt(packet);
		DEBUG(verify_part);
		part++;
		if(rlen < FILE_PART_LIMIT){
			break;
		}
	}

	if(close(filefd) < 0){
		std::cout << "Uh oh, close file error." << std::endl;
		return 1;
	}

	std::cout << file_path << " has been sent." << std::endl;

	return 0;
}

int recv_file_routine(int socketfd){
	int filefd;
	ssize_t len;
	char packet[PACKET_LIMIT];
	std::string file_part;
	std::string file_path;

	if((len = read(socketfd, packet, PACKET_LIMIT)) < 0){
		std::cout << "Uh oh, read method error." << std::endl;
		return 1;
	}
	packet[len] = 0;
	file_path = "transfers/" + decrypt(packet);
	std::cout << file_path << std::endl;

	if((filefd = open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) < 0){
		std::cout << "Uh oh, open error!" << std::endl;
		return 1;
	}

	file_part = "File is being transferred!";
	if(send(socketfd, file_part)){
		std::cout << "Uh oh, send file error!" << std::endl;
		return 1;
	}

	int part = 0;
	while(1){
		if((len = read(socketfd, packet, PACKET_LIMIT)) < 0){
			std::cout << "Uh oh, read file part error." << std::endl;
			break;
		}
		packet[len] = 0;
		try{
			file_part = decrypt(packet);
		}catch(const CryptoPP::Exception& e){
			std::cout << e.what() << std::endl;
			std::cout << part << std::endl;
			break;
		}

		if((len = write(filefd, file_part.c_str(), static_cast<size_t>(file_part.length()))) < 0){
			std::cout << "Uh oh, write file part error!" << std::endl;
			break;
		}
		file_part = "Part " + std::to_string(part) + " success!";
		if(send(socketfd, file_part)){
			std::cout << "Uh oh, send part verify error." << std::endl;
			break;
		}
		part++;
		if(len < FILE_PART_LIMIT){
			break;
		}
	}

	if(close(filefd) < 0){
		std::cout << "Uh oh, close file error." << std::endl;
		return 1;
	}

	std::cout << file_path << " has been received." << std::endl;

	return 0;
}
