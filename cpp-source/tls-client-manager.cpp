#include "util.hpp"
#include "tls-client-manager.hpp"

TlsClientManager::TlsClientManager(bool new_verbose)
:name("TlsClientManager"),
verbose(new_verbose){}

bool TlsClientManager::send(SSL* ssl, const char* data, size_t data_length){
	int len = 0, err = SSL_ERROR_WANT_WRITE;
	std::chrono::milliseconds send_start = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	std::chrono::milliseconds diff = std::chrono::milliseconds(0);
	std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

	while(err == SSL_ERROR_WANT_WRITE){
		if(diff > timeout){
			PRINT("Client SSL_write timeout...")
			return true;
		}

		len = SSL_write(ssl, data, static_cast<int>(data_length));
		err = SSL_get_error(ssl, len);
		PRINT("SENT " << len << " BYTES")

		diff = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()) - send_start;
	}
	switch(err){
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_WANT_WRITE:
			PRINT("CLIENT WANT WRITE " << ssl)
			// Nothing to read, nonblocking mode.
			return 0;
		case SSL_ERROR_ZERO_RETURN:
			ERROR("CLIENT write zero " << ssl)
			return true;
		case SSL_ERROR_SYSCALL:
			PRINT(this->name << ": CLIENT write SSL_ERROR_SYSCALL")
			ERR_print_errors_fp(stdout);
			return true;
		default:
			ERROR("CLIENT other SSL_write " << err << " from " << ssl)
			ERR_print_errors_fp(stdout);
			return true;
	}
	
	//DEBUG("SSL_write took milliseconds: " << diff.count())
	if(len != static_cast<int>(data_length)){
		ERROR("Invalid number of bytes written...")
	}
	return false;
}

ssize_t TlsClientManager::recv(SSL* ssl, char* data, size_t data_length){
	int len = 0, err = SSL_ERROR_WANT_READ;
	std::chrono::milliseconds recv_start = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	std::chrono::milliseconds diff = std::chrono::milliseconds(0);
	std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

	while(err == SSL_ERROR_WANT_READ){
		if(diff > timeout){
			PRINT("CLIENT SSL_read timeout...")
			return true;
		}

		len = SSL_read(ssl, data, static_cast<int>(data_length));
		err = SSL_get_error(ssl, len);

		if(err != SSL_ERROR_WANT_READ){
			PRINT(err)
			ERR_print_errors_fp(stdout);
		}

		diff = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()) - recv_start;
	}
	switch(err){
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_WANT_READ:
			PRINT("CLIENT WANT READ " << ssl)
			// Nothing to read, nonblocking mode.
			return 0;
		case SSL_ERROR_ZERO_RETURN:
			ERROR("CLIENT server read zero " << ssl)
			return -2;
		case SSL_ERROR_SYSCALL:
			PRINT(this->name << ": CLIENT read SSL_ERROR_SYSCALL")
			ERR_print_errors_fp(stdout);
			return -3;
		default:
			ERROR("CLIENT other SSL_read " << err << " from " << ssl)
			ERR_print_errors_fp(stdout);
			return -1;
	}
	
	//DEBUG("SSL_read took milliseconds: " << diff.count())
	data[len] = 0;
	return len;
}

bool TlsClientManager::post(uint16_t port, std::string hostname, std::string path, std::unordered_map<std::string, std::string> headers, std::string body, char* response){
	std::string request = "POST " + path + " HTTP/1.1\n";
	request += "Host: " + hostname + "\n";
	request += "Connection: close\n";
	request += "Accept: */*\n";

	for(auto iter = headers.begin(); iter != headers.end(); ++iter){
		request += iter->first + ": " + iter->second + "\n";
	}

	Util::replace_all(body, "\r\n", "<br>");
	
	request += "Content-Length: " + std::to_string(body.length()) + "\r\n\r\n";
	request += body;

	return this->communicate(hostname, port, request.c_str(), request.length(), response);
}

bool TlsClientManager::get(uint16_t port, std::string hostname, std::string path, char* response){
	std::string request = "GET " + path + " HTTP/1.1\n";
	request += "Host: " + hostname + "\n";
	request += "Connection: close\n";
	request += "Accept: */*\r\n\r\n";

	return this->communicate(hostname, port, request.c_str(), request.length(), response);
}

bool TlsClientManager::communicate(std::string hostname, uint16_t port, const char* request, size_t length, char* response){
	int fd;
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("reconnect socket");
		return true;
	}

	// Reuse the host for the duration of a stable connection.
	if(!this->host_addresses.count(hostname)){
		struct hostent* host;
		if((host = gethostbyname(hostname.c_str())) == 0){
			perror("gethostbyname");
			return true;
		}
		this->host_addresses[hostname] = *reinterpret_cast<struct in_addr*>(host->h_addr);
	}

	struct sockaddr_in* server_address = new struct sockaddr_in();
	bzero(&(server_address->sin_zero), 8);
	server_address->sin_family = AF_INET;
	server_address->sin_addr = this->host_addresses[hostname];
	server_address->sin_port = htons(port);

	while(true){
		if(connect(fd, reinterpret_cast<struct sockaddr*>(server_address), sizeof(struct sockaddr_in)) < 0){
			if(errno !=  EINPROGRESS && errno != EALREADY){
				perror("connect");
				return true;
			}
			// Still connecting, cooooool.
		}else{
			break;
		}
	}
	SSL_library_init();
	SSLeay_add_ssl_algorithms();
	SSL_load_error_strings();
	const SSL_METHOD *meth = TLSv1_2_client_method();
	SSL_CTX *ctx = SSL_CTX_new(meth);
	SSL* ssl = SSL_new(ctx);
	if (!ssl) {
		perror("SSL_new");
		return true;
	}
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

	SSL_set_fd(ssl, fd);
	int err = SSL_connect(ssl);
	if (err <= 0) {
		perror("SSL_connect");
		return true;
	}

	PRINT("SSL connection using " << SSL_get_cipher(ssl))

	Util::set_non_blocking(fd);

	if(this->send(ssl, request, length)){
		SSL_free(ssl);
		PRINT("Client problem sending...")
		return true;
	}

	if(this->recv(ssl, response, PACKET_LIMIT) < 0){
		SSL_free(ssl);
		PRINT("Client problem receiving...")
		return true;
	}

	SSL_free(ssl);
	return false;
}

TlsClientManager::~TlsClientManager(){
	if(this->verbose){
		PRINT(this->name + " delete")
	}
}
