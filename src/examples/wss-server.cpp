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


class WebSocketFrame{
public:
	uint8_t opcode;
	uint8_t fin;
	uint8_t masked;
	int payload_len;
};

static WebSocketFrame* parse_web_socket_frame(const uint8_t* request){
	WebSocketFrame* f = new WebSocketFrame();
	f->opcode = request[0] & 0x0F;
	f->opcode = (request[0] >> 7) & 0x01;
	f->opcode = (request[1] >> 7) & 0x01;
	
	return f;
}

/*
	if(in_length < 3) return INCOMPLETE_FRAME;

	unsigned char msg_opcode = in_buffer[0] & 0x0F;
	unsigned char msg_fin = (in_buffer[0] >> 7) & 0x01;
	unsigned char msg_masked = (in_buffer[1] >> 7) & 0x01;

	// *** message decoding 

	int payload_length = 0;
	int pos = 2;
	int length_field = in_buffer[1] & (~0x80);
	unsigned int mask = 0;

	//printf("IN:"); for(int i=0; i<20; i++) printf("%02x ",buffer[i]); printf("\n");

	if(length_field <= 125) {
		payload_length = length_field;
	}
	else if(length_field == 126) { //msglen is 16bit!
		payload_length = in_buffer[2] + (in_buffer[3]<<8);
		pos += 2;
	}
	else if(length_field == 127) { //msglen is 64bit!
		payload_length = in_buffer[2] + (in_buffer[3]<<8); 
		pos += 8;
	}
	//printf("PAYLOAD_LEN: %08x\n", payload_length);
	if(in_length < payload_length+pos) {
		return INCOMPLETE_FRAME;
	}

	if(msg_masked) {
		mask = *((unsigned int*)(in_buffer+pos));
		//printf("MASK: %08x\n", mask);
		pos += 4;

		// unmask data:
		unsigned char* c = in_buffer+pos;
		for(int i=0; i<payload_length; i++) {
			c[i] = c[i] ^ ((unsigned char*)(&mask))[i%4];
		}
	}
	
	if(payload_length > out_size) {
		//TODO: if output buffer is too small -- ERROR or resize(free and allocate bigger one) the buffer ?
	}

	memcpy((void*)out_buffer, (void*)(in_buffer+pos), payload_length);
	out_buffer[payload_length] = 0;
	*out_length = payload_length+1;
	
	//printf("TEXT: %s\n", out_buffer);

	if(msg_opcode == 0x0) return (msg_fin)?TEXT_FRAME:INCOMPLETE_TEXT_FRAME; // continuation frame ?
	if(msg_opcode == 0x1) return (msg_fin)?TEXT_FRAME:INCOMPLETE_TEXT_FRAME;
	if(msg_opcode == 0x2) return (msg_fin)?BINARY_FRAME:INCOMPLETE_BINARY_FRAME;
	if(msg_opcode == 0x9) return PING_FRAME;
	if(msg_opcode == 0xA) return PONG_FRAME;

	return ERROR_FRAME;
*/

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
			WebSocketFrame* frame = parse_web_socket_frame(reinterpret_cast<const uint8_t*>(data));
		}else{
			PRINT("RECV: " << data)
		}
		return data_length;
	};

	server.run(false, 1);
}
