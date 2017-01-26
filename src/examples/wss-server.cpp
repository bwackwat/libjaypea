#include "cryptopp/sha.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "json.hpp"
#include "util.hpp"
#include "tcp-server.hpp"

#define WSS_MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static std::string hash_key_sha1(std::string token){
	PRINT("ACT1|" << token << '|')
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
	PRINT("ACT|" << accept << "|")
	return "HTTP/1.1 101 Switching Protocols\n"
		"Upgrade: websocket\n"
		"Connection: Upgrade\n"
		"Sec-WebSocket-Accept: " + accept + "\n"
//		"Sec-WebSocket-Protocol: libjaypea\n"
//		"Server: libjaypea\n"
//		"Access-Control-Allow-Origin: *\n"
//		"Access-Control-Allow-Credentials: true\n"
//		"Access-Control-Allow-Headers: content-type\n"
//		"Access-Control-Allow-Headers: authorization\n"
//		"Access-Control-Allow-Headers: x-websocket-version\n"
//		"Access-Control-Allow-Headers: x-websocket-protocol\n"
//		"Access-Control-Allow-Headers: x-websocket-extensions"
		"\r\n\r\n\r\n";
}

int main(){
	EpollServer server(11000, 10);

	PRINT(hash_key_sha1(std::string("dGhlIHNhbXBsZSBub25jZQ==") + WSS_MAGIC_GUID))

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT("RECV: " << data)
		JsonObject r_obj(OBJECT);
		Util::parse_http_api_request(data, &r_obj);
		PRINT("JPON: " << r_obj.stringify(true))

		if(r_obj.HasObj("Sec-WebSocket-Key", STRING)){
			std::string key = r_obj.GetStr("Sec-WebSocket-Key");
			PRINT(std::hex << key)
			PRINT("APLESPLESKEY|" << key << '|')
			PRINT("APLESPLESKEY|" << r_obj.GetStr("Sec-WebSocket-Key") << '|')
			PRINT("APLESPLESKEY|" << r_obj.GetStr("Origin") << '|')
			PRINT("APLESPLESKEY|" << key << '|')
			const std::string response = wss_handshake_response(r_obj.GetStr("Sec-WebSocket-Key"));
			if(server.send(fd, response.c_str(), response.length())){
				return -1;
			}

			PRINT("DELI: " << response)
		}else{
			PRINT("Bad websocket request.")
			return -1;
		}

		return data_length;
	};

	server.run(false, 1);
}
