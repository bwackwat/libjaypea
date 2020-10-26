#include "util.hpp"
#include "tls-client-manager.hpp"

TlsClientManager::TlsClientManager(bool new_verbose)
:name("TlsClientManager"),
verbose(new_verbose){
}

bool TlsClientManager::post(uint16_t port, std::string hostname, std::string path, std::unordered_map<std::string, std::string> headers, std::string body, char* response){
	std::string request = "POST " + path + " HTTP/1.1\r\n";
	request += "Host: " + hostname + "\r\n";
	request += "Connection: close\r\n";
	request += "User-Agent: libjaypea\r\n";
	request += "Accept: */*\r\n";

	for(auto iter = headers.begin(); iter != headers.end(); ++iter){
		request += iter->first + ": " + iter->second + "\n";
	}

	Util::replace_all(body, "\r\n", "<br>");
	
	request += "Content-Length: " + std::to_string(body.length()) + "\r\n\r\n";
	request += body;

	return this->communicate(hostname, port, request.c_str(), request.length(), response);
}

std::string TlsClientManager::get_body(uint16_t port, std::string hostname, std::string path){
	std::string request = "GET " + path + " HTTP/1.1\r\n";
	request += "Host: " + hostname + "\r\n";
	request += "Connection: close\r\n";
	request += "User-Agent: libjaypea\r\n";
	request += "Accept: */*\r\n\r\n";

	char response[PACKET_LIMIT];

	if(this->communicate(hostname, port, request.c_str(), request.length(), response)){
		return "{\"error\":\"Failed to communicate to host " + hostname + ".\"}";
	}

	std::string response_str = std::string(response);
	std::size_t body_separator = response_str.find("\r\n\r\n", 0);

	if(body_separator == std::string::npos){
		return response_str;
	}

	return response_str.substr(body_separator + 4, response_str.length());
}

std::string TlsClientManager::get(std::string url){
	if(Util::startsWith(url, "http://")){
		PRINT("Only https:// is supported.")
		throw new std::exception();
	}
	if(Util::startsWith(url, "https://")){
		url = url.substr(8);
	}

	std::size_t slash = url.find('/', 0);
	std::string hostname = url.substr(0, slash);
	std::string path = url.substr(slash);

	return this->get_body(443, hostname, path);
}

bool TlsClientManager::communicate(std::string hostname, uint16_t port, const char* request, size_t length, char* response){
	int fd;
	ssize_t len;
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("reconnect socket");
		return true;
	}

	struct hostent* host;
	if((host = gethostbyname(hostname.c_str())) == nullptr){
		perror("gethostbyname");
		return true;
	}

	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(struct sockaddr_in));
	server_address.sin_family = AF_INET;
	server_address.sin_addr = *reinterpret_cast<struct in_addr*>(host->h_addr);
	server_address.sin_port = htons(port);

	if(inet_pton(AF_INET, hostname.c_str(), &(server_address.sin_addr)) < 0){
		perror("inet_pton");
		return true;
	}

	//if(connect(fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0){
	if(connect(fd, reinterpret_cast<struct sockaddr*>(&server_address), sizeof(server_address)) < 0){
		perror("connect");
		return true;
	}

	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
	SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_SSLv2);
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

	SSL* ssl = SSL_new(ctx);
	if (!ssl) {
		perror("SSL_new");
		return true;
	}

	SSL_set_fd(ssl, fd);
	int err = SSL_connect(ssl);
	if (err < 0) {
		perror("SSL_connect");
		return true;
	}

	PRINT("SSL connection using " << SSL_get_cipher(ssl))

	//Util::set_non_blocking(fd);

	if(SSL_write(ssl, request, static_cast<int>(length)) < 0){
		perror("SSL_write");
		return true;
	}

	std::chrono::milliseconds recv_start = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	std::chrono::milliseconds diff = std::chrono::milliseconds(0);
	std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);
	char buffer[PACKET_LIMIT];
	int offset = 0;
	do{
		if(diff > timeout){
			PRINT("CLIENT SSL_read timeout...")
			break;
		}
		len = SSL_read(ssl, buffer, PACKET_LIMIT);
		err = SSL_get_error(ssl, static_cast<int>(len));
		switch(err){
		case SSL_ERROR_NONE: 
			//printf("SSL_ERROR_NONE\n");
			break;
		case SSL_ERROR_WANT_READ:
			printf("SSL_ERROR_WANT_READ\n");
			break;
		case SSL_ERROR_WANT_WRITE:
			printf("SSL_ERROR_WANT_WRITE\n");
			// This happens when the server thinks we want to keep the connection open... Close it!
			len = 0;
			break;
		case SSL_ERROR_ZERO_RETURN:
			printf("SSL_ERROR_ZERO_RETURN\n");  
			break;
		default:
			break;
		}

		if((len > 0) && (err == SSL_ERROR_NONE)){
			buffer[len] = 0;
			memcpy(response + offset, buffer, static_cast<size_t>(len));
			offset += len;
			response[offset] = 0;
		}else if((len < 0)  && (err == SSL_ERROR_WANT_READ)){
			printf("len < 0 \n");
			if (errno != EAGAIN)
			{
				printf("len < 0 errno != EAGAIN \n");
				perror ("read");
			}
			break;
		}else if(len == 0){
			break;
		}

		diff = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()) - recv_start;
	}while(ssl && len > 0);

	DEBUG("COMMUNICATED WITH " << hostname)

	SSL_free(ssl);
	return false;
}

TlsClientManager::~TlsClientManager(){
	if(this->verbose){
		PRINT(this->name + " delete")
	}
}
