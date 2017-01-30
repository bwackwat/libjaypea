#include "cryptopp/sha.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "json.hpp"
#include "util.hpp"
#include "tcp-server.hpp"

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
	LOGGING,
	PLAYING
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

	uint64_t len = request[1] & 0x7f;
	uint64_t payload_length;
	int offset = 2;

	if(len <= 125){
		payload_length = len;
	}else if(len == 126){
		payload_length = request[2] + (request[3] << 8);
		offset += 2;
	}else if(len == 127){
		payload_length = request[2] + (request[3] << 8);
		offset += 8;
	}else{
		ERROR("whack length field");
	}

	if(data_length < payload_length + offset){
		ERROR("incorrect or incomplete length field")
	}

	if(request[1] & 0x80){
		int mask = *static_cast<unsigned int*>(request + offset);
		offset += 4;

		for(const char* it = request + offset; it < data_length; ++ it){
			message << *it ^ static_cast<unsigned char*>(&mask)[i % 4];
		}
	}

	return message.str();
}

int main(){
	std::unordered_map<std::string, Player*> player_data;
	std::unordered_map<int /* fd */, std::string /* logged in handle */> client_player;
	std::unordered_map<int /* fd */, enum ClientState> client_state;

	EpollServer server(11000, 10);

//	PRINT(hash_key_sha1(std::string("dGhlIHNhbXBsZSBub25jZQ==") + WSS_MAGIC_GUID))

	server.on_connect =  [&](int fd){
		client_state[fd] = CONNECTING;
	};

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		if(client_state[fd] == CONNECTING){
			PRINT("RECV: " << data)
			JsonObject r_obj(OBJECT);
			Util::parse_http_api_request(data, &r_obj);
			PRINT("JPON: " << r_obj.stringify(true))

			if(r_obj.HasObj("Sec-WebSocket-Key", STRING)){
				const std::string response = wss_handshake_response(r_obj.GetStr("Sec-WebSocket-Key"));
				if(server.send(fd, response.c_str(), response.length())){
					return -1;
				}

				PRINT("DELI: " << response)
				client_state[fd] = LOGGING;
			}else{
				PRINT("Bad websocket request.")
				return -1;
			}
		}else if(client_state[fd] == LOGGING){
			PRINT("RECV: " << data)
			std::string message = parse_web_socket_frame(data, data_length);
			PRINT("MSG: " << message)
		}else{
			PRINT("RECV: " << data)
		}
		return data_length;
	};

	server.run(false, 1);
}
