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
	return "HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: " + accept + "\r\n"
		"\r\n";
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

static void print_bits(const char* data, size_t data_length){
	std::bitset<8> bits;
	for(size_t i = 0; i < data_length; ++i){
		bits = std::bitset<8>(data[i]);
		std::cout << bits << ' ';
		if(i % 4 == 3){
			std::cout << std::endl;
		}
	}
	std::cout << std::endl;
}

static std::string parse_web_socket_frame(const char* data, ssize_t data_length){
	std::stringstream message;

	uint64_t len = data[1] & 0x7F;
	uint64_t payload_length;
	size_t offset;

	if(len == 126){
		payload_length = static_cast<uint64_t>(data[2]) + (static_cast<uint64_t>(data[3]) << 8);
		offset = 4;
	}else if(len == 127){
		payload_length = static_cast<uint64_t>(data[2]) + (static_cast<uint64_t>(data[3]) << 8);
		offset = 10;
	}else{
		payload_length = len;
		offset = 2;
	}

	if(data_length < payload_length + offset){
		ERROR("DIDNT READ ALL DATA")
	}

	unsigned char mask[4] = {0, 0, 0, 0};
	if(data[1] & 0x80){
		std::memcpy(mask, data + offset, 4);
		offset += 4;
	}

	for(int i = 0; i < payload_length; ++i){
		message << static_cast<char>(static_cast<unsigned char>(data[offset + i]) ^ mask[i % 4]);
	}

	#if defined(_DO_DEBUG)
	PRINT("DATA:")
	print_bits(data, data_length);
	PRINT("MASK:")
	print_bits(reinterpret_cast<char*>(mask), 4);
	#endif

	return message.str();
}

static std::string create_web_socket_frame(std::string message){
	char frame[message.length() + 10];
	char ssize[2] = {0, 0};
	char lsize[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	size_t offset;

	// Basic binary frame.
	frame[0] = 0x82;
	if(message.length() <= 125){
		frame[1] = static_cast<unsigned char>(message.length());
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
			//if(server->send(fd, data, data_length)){
			PRINT("SEND: " << response)
			print_bits(response.c_str(), response.length());
			if(server->send(fd, response.c_str(), response.length())){
				return -1;
			}
			PRINT("RECV: " << message)
		}else{
			PRINT("NEVER")
		}
		return data_length;
	};

	server->run(false, 1);
}
