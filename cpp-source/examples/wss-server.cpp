#include <bitset>

#include "cryptopp/sha.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "json.hpp"
#include "util.hpp"
#include "tcp-server.hpp"
#include "tls-epoll-server.hpp"

#define WSS_MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static std::string hash_key_sha1(std::string token){
	std::string digest;
	CryptoPP::SHA1 hash;
	CryptoPP::StringSource source(token, true,
		new CryptoPP::HashFilter(hash,
		new CryptoPP::Base64Encoder(
		new CryptoPP::StringSink(digest), false)));
	return digest;
}

static std::string wss_handshake_response(std::string key){
	std::string accept = hash_key_sha1(key + WSS_MAGIC_GUID);
	return "HTTP/1.1 101 Switching Protocols\n"
		"Upgrade: websocket\n"
		"Connection: Upgrade\n"
		"Sec-WebSocket-Accept: " + accept + "\n"
		"\r\n\r\n";
}

enum ClientState {
	CONNECTING,
	EXCHANGING
};

class Player{
public:
	std::string secret;
	int x;
	int y;
	int gold;
	
	Player(std::string new_secret)
	:secret(new_secret){
		
	}
};

static std::string parse_web_socket_frame(const char* request, ssize_t data_length){
	std::stringstream message;

	uint64_t len = request[1] & 0x7F;
	uint64_t payload_length;
	size_t offset;

	std::bitset<8> bits;
	for(int i = 0; i < data_length; ++i){
		bits = std::bitset<8>(request[i]);
		std::cout << bits << ' ';
		if(i % 4 == 3){
			std::cout << std::endl;
		}
	}
	std::cout << std::endl;

	if(len == 126){
		payload_length = request[2] + (request[3] << 8);
		offset = 4;
	}else if(len == 127){
		payload_length = request[2] + (request[3] << 8);
		offset = 10;
	}else{
		payload_length = len;
		offset = 2;
	}

	PRINT("PAYLOAD LEN: " << payload_length)
	PRINT("RECV DATA LEN: " << data_length)

	if(data_length < payload_length + offset){
		ERROR("incorrect or incomplete length field")
	}

	unsigned char mask[4] = {0, 0, 0, 0};
	if(request[1] & 0x80){
		//std::memcpy(mask, request + offset, 4);
		mask[0] = static_cast<unsigned char>(request[offset]);
		mask[1] = static_cast<unsigned char>(request[offset + 1]);
		mask[2] = static_cast<unsigned char>(request[offset + 2]);
		mask[3] = static_cast<unsigned char>(request[offset + 3]);
		offset += 4;
	}

	PRINT("OFFSET: " << offset)

	PRINT("MASK: ")
	bits = std::bitset<8>(mask[0]);
	std::cout << bits;
	bits = std::bitset<8>(mask[1]);
	std::cout << bits;
	bits = std::bitset<8>(mask[2]);
	std::cout << bits;
	bits = std::bitset<8>(mask[3]);
	std::cout << bits << std::endl;

	int i = 0;
	for(const char* it = request + offset; *it; ++it){
		message << static_cast<char>(static_cast<unsigned char>(*it) ^ mask[i++ % 4]);
	}

	return message.str();
}

static std::string create_web_socket_frame(std::string message){
	char frame[message.length() + 10];
	char ssize[2] = {0, 0};
	char lsize[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	size_t offset;

	// Basic text frame.
	frame[0] = 1;
	if(message.length() <= 125){
		frame[1] = static_cast<char>(message.length());
		offset = 2;
	}else if(message.length() <= 65535){
		*(reinterpret_cast<uint16_t*>(ssize)) = static_cast<uint16_t>(message.length());
		frame[1] = 126;
		frame[2] = ssize[0];
		frame[3] = ssize[1];
		offset = 4;
	}else{
		*(reinterpret_cast<uint64_t*>(lsize)) = static_cast<uint64_t>(message.length());
		frame[1] = 127;
		frame[2] = lsize[0];
		frame[3] = lsize[1];
		frame[4] = lsize[2];
		frame[5] = lsize[3];
		frame[6] = lsize[4];
		frame[7] = lsize[5];
		frame[8] = lsize[6];
		frame[9] = lsize[7];
		offset = 10;
	}

	std::memcpy(frame + offset, message.c_str(), message.length());

	return std::string(frame, message.length() + offset);
}

/*

static std::string create_web_socket_frame(char* data, ssize_t data_length){
	int pos = 0;
	int size = data_length; 
	buffer[pos++] = (unsigned char)frame_type; // text frame

	if(size <= 125) {
		buffer[pos++] = size;
	}
	else if(size <= 65535) {
		buffer[pos++] = 126; //16 bit length follows
		
		buffer[pos++] = (size >> 8) & 0xFF; // leftmost first
		buffer[pos++] = size & 0xFF;
	}
	else { // >2^16-1 (65535)
		buffer[pos++] = 127; //64 bit length follows
		
		// write 8 bytes length (significant first)
		
		// since msg_length is int it can be no longer than 4 bytes = 2^32-1
		// padd zeroes for the first 4 bytes
		for(int i=3; i>=0; i--) {
			buffer[pos++] = 0;
		}
		// write the actual 32bit msg_length in the next 4 bytes
		for(int i=3; i>=0; i--) {
			buffer[pos++] = ((size >> 8*i) & 0xFF);
		}
	}
	memcpy((void*)(buffer+pos), msg, size);
	return (size+pos);
}

*/

int main(int argc, char** argv){
	std::string ssl_certificate;
	std::string ssl_private_key;
	int port;
	bool http = false;

	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("http", &http);
	Util::parse_arguments(argc, argv, "This is a (Secure) WebSocketServer");
	
	std::unordered_map<std::string, Player*> player_data;
	std::unordered_map<int /* fd */, std::string /* logged in handle */> client_player;
	std::unordered_map<int /* fd */, enum ClientState> client_state;

	
	EpollServer* server;
	if(http){
		server = new EpollServer(static_cast<uint16_t>(port), 10);
	}else{
		server = new TlsEpollServer(ssl_certificate, ssl_private_key, static_cast<uint16_t>(port), 10);
	}

	server->on_connect =  [&](int fd){
		client_state[fd] = CONNECTING;
	};

	server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT("RECV: " << data)
		if(client_state[fd] == CONNECTING){
			JsonObject r_obj(OBJECT);
			Util::parse_http_api_request(data, &r_obj);
			PRINT("JPON: " << r_obj.stringify(true))

			if(r_obj.HasObj("Sec-WebSocket-Key", STRING)){
				const std::string response = wss_handshake_response(r_obj.GetStr("Sec-WebSocket-Key"));
				if(server->send(fd, response.c_str(), response.length())){
					return -1;
				}

				PRINT("DELI: " << response)
				client_state[fd] = EXCHANGING;
			}else{
				PRINT("Bad websocket request.")
				return -1;
			}
		}else if(client_state[fd] == EXCHANGING){
			std::string message = parse_web_socket_frame(data, data_length);
			std::string response = create_web_socket_frame(std::string("I got the message!"));
			if(server->send(fd, data, data_length)){
			//if(server.send(fd, response.c_str(), response.length())){
				return -1;
			}
			PRINT("MSG: " << message)
		}else{
			PRINT("NEVER")
		}
		return data_length;
	};

	server->run(false, 1);
}
