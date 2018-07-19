#pragma once

#include "cryptopp/sha.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/cryptlib.h"

#include "util.hpp"
#include "tcp-server.hpp"

class Websocket{
private:
	EpollServer* server;

	std::string parse_frame(const char* data, size_t data_length){
		std::stringstream message;
		uint64_t len = data[1] & 0x7F;
		uint64_t payload_length;
		size_t offset;

		if(len == 126){
			payload_length = *(reinterpret_cast<const uint16_t*>(data + 2));
			//payload_length = static_cast<uint16_t>(data[2]) + (static_cast<uint16_t>(data[3]) << 8);
			offset = 4;
		}else if(len == 127){
			//TODO: read uint64_t not uint16_t
			payload_length = static_cast<uint64_t>(data[2]) + (static_cast<uint64_t>(data[3]) << 8);
			offset = 10;
		}else{
			payload_length = len;
			offset = 2;
		}

		if(PACKET_LIMIT < payload_length + offset){
			ERROR("DIDNT READ ALL DATA")
			PRINT("L:" << len)
			PRINT("DATAL:" << data_length)
			PRINT("PAYLL:" << payload_length)
			PRINT("OFFSE:" << offset)
		}

		unsigned char mask[4] = {0, 0, 0, 0};
		if(data[1] & 0x80){
			std::memcpy(mask, data + offset, 4);
			offset += 4;
		}

		for(size_t i = 0; i < payload_length && offset + i < data_length; ++i){
			message << static_cast<char>(static_cast<unsigned char>(data[offset + i]) ^ mask[i % 4]);
		}

		#if defined(DO_DEBUG)
			PRINT("DATA:")
			Util::print_bits(data, data_length);
			PRINT("MASK:")
			Util::print_bits(reinterpret_cast<char*>(mask), 4);
		#endif

		return message.str();
	}

	std::string get_handshake_response(std::string key){
		std::string accept_hash;
		CryptoPP::SHA1 hasher;
		CryptoPP::StringSource source(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", true,
			new CryptoPP::HashFilter(hasher,
			new CryptoPP::Base64Encoder(
			new CryptoPP::StringSink(accept_hash), false)));
		return "HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: " + accept_hash + "\r\n"
			"\r\n";
	}
public:
	Websocket(EpollServer* new_server)
	:server(new_server){}

	std::unordered_map<int, bool> client_handshake_complete;

	std::string create_frame(const char* data, size_t data_length){
		//char frame[data_length + 10];
		std::stringstream frame;
		char lsize[8] = {0, 0, 0, 0, 0, 0, 0, 0};
		size_t offset;

		// Basic binary frame.
		//frame[0] = static_cast<char>(0x81);
		frame << static_cast<char>(0x81);
		if(data_length <= 125){
			frame << static_cast<char>(data_length);
			//frame[1] = static_cast<char>(data_length);
			offset = 2;
		}else if(data_length <= 65535){
			DEBUG("DL:" << data_length)
			frame << static_cast<char>(126);
			//frame[1] = 126;
			frame << static_cast<char>((static_cast<uint16_t>(data_length) >> 8) & 0xFF);
			//frame[2] = static_cast<char>((static_cast<uint16_t>(data_length) >> 8) & 0xFF);
			frame << static_cast<char>(static_cast<uint16_t>(data_length) & 0xFF);
			//frame[3] = static_cast<char>(static_cast<uint16_t>(data_length) & 0xFF);
			offset = 4;
		}else{
			*(reinterpret_cast<uint64_t*>(lsize)) = static_cast<uint64_t>(data_length);
			/*frame[1] = 127;
			frame[2] = lsize[0];
			frame[3] = lsize[1];
			frame[4] = lsize[2];
			frame[5] = lsize[3];
			frame[6] = lsize[4];
			frame[7] = lsize[5];
			frame[8] = lsize[6];
			frame[9] = lsize[7];*/
			offset = 10;
		}

		frame << std::string(data, data_length);
		//std::memcpy(frame + offset, data, data_length);
		//frame[data_length + offset] = 0;
		
		#if defined(DO_DEBUG)
			PRINT("SDATA:")
			Util::print_bits(frame.str().c_str(), frame.str().length());
		#endif

		return frame.str();
	}

	ssize_t recv(int fd, const char* data, size_t data_length){
		if(this->client_handshake_complete[fd]){
			std::string message = this->parse_frame(data, data_length);
			PRINT("RECV:" << message)
			if(message == "ping"){
				message = "pong";
				if(this->server->send(fd, message.c_str(), message.length())){
					DEBUG("Send handshake failed.")
					return -1;
				}
				return static_cast<ssize_t>(message.length());
			}
			PRINT("WEBSOCKET ONREAD")
			return this->server->on_read(fd, message.c_str(), static_cast<size_t>(message.length()));
		}

		JsonObject request_obj(OBJECT);
		Util::parse_http_api_request(data, &request_obj);
		//DEBUG("JPON: " << request_obj.stringify(true))
		if(!request_obj.HasObj("Sec-WebSocket-Key", STRING)){
			DEBUG("Bad websocket request.")
			return -1;
		}

		std::string response = this->get_handshake_response(request_obj.GetStr("Sec-WebSocket-Key"));
		if(this->server->send(fd, response.c_str(), response.length())){
			DEBUG("Send handshake failed.")
			return -1;
		}
		DEBUG("DELI: " << response)
		this->client_handshake_complete[fd] = true;
		return 1;
	}
};
