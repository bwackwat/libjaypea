#include <csignal>

#include "util.hpp"
#include "tls-epoll-server.hpp"

/**
 * /implements EpollServer
 *
 * @brief This class has the same characteristics of EpollServer, yet uses OpenSSL private-public key encryption.
 *
 * @param certificate The path to the certificate chain file needed by OpenSSL.
 * @param private_key The path to the private key file needed by OpenSSL.
 * @param port See EpollServer::EpollServer.
 * @param max_connections See EpollServer::EpollServer.
 *
 * Disables OpenSSL SSLv3.
 * Users the cipher list: "ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS"
 */
TlsEpollServer::TlsEpollServer(std::string certificate, std::string private_key, uint16_t port, size_t max_connections, std::string name)
:EpollServer(port, max_connections, name){
	SSL_library_init();
	SSL_load_error_strings();

	if((this->ctx = SSL_CTX_new(SSLv23_server_method())) == 0){
		ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_new");
	}
	
	if(SSL_CTX_set_ecdh_auto(this->ctx, 1) == 0){
		ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_set_ecdh_auto");
	}

	if(SSL_CTX_use_certificate_chain_file(this->ctx, certificate.c_str()) != 1){
		ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_use_certificate_file");
	}

	if(SSL_CTX_use_PrivateKey_file(this->ctx, private_key.c_str(), SSL_FILETYPE_PEM) != 1){
		ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_use_PrivateKey_file");
	}

	std::signal(SIGPIPE, SIG_IGN);

	// Hell yeah, use ECDH.
	if(!SSL_CTX_set_ecdh_auto(this->ctx, 1)){
		ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_set_ecdh_auto");
	}

	// SSLv3 is insecure via poodles.
	SSL_CTX_set_options(this->ctx, SSL_OP_NO_SSLv3);

	// "Good" cipher list from the interweb.
	if(!SSL_CTX_set_cipher_list(this->ctx, "ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS")){
		ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_set_cipher_list");
	}

//	PRINT("SSL Mode: " << SSL_CTX_set_mode(this->ctx, SSL_MODE_ENABLE_PARTIAL_WRITE))
}

/**
 * @brief Does an SSL_free before closing the socket.
 *
 * See EpollServer::close_client
 */
void TlsEpollServer::close_client(int* fd, std::function<void(int*)> callback){
	PRINT("SSL_free'd " << *fd)
	SSL_free(this->client_ssl[*fd]);
	callback(fd);
}

/**
 * @brief Uses SSL_write instead of a regular write.
 *
 * See EpollServer::send
 */ 
bool TlsEpollServer::send(int fd, const char* data, size_t data_length){
	int len = 0, err = SSL_ERROR_WANT_WRITE, count = 0;
	//TODO: A malicious SSL client could continously request the same bytes (keep ACKing received bytes).
	while(err == SSL_ERROR_WANT_WRITE){
		len = SSL_write(this->client_ssl[fd], data, static_cast<int>(data_length));
		err = SSL_get_error(this->client_ssl[fd], len);
		count++;
	}
	if(err != SSL_ERROR_NONE){
		ERROR("SSL_write: " << err)
		return true;
	}
	PRINT("SSL_write: " << count)
	if(len != static_cast<int>(data_length)){
		ERROR("Invalid number of bytes written...")
	}
	this->write_counter[fd]++;
	return false;
}

ssize_t TlsEpollServer::recv(int fd, char* data, size_t data_length){
	return this->recv(fd, data, data_length, this->on_read);
}

/**
 * @brief This is useful for HTTPS multi-part requests.
 * Google chrome requires this funtionality to piece together POST headers and body.
 *
 * See EpollServer::recv
 */
ssize_t TlsEpollServer::recv(int fd, char* data, size_t data_length,
std::function<ssize_t(int, char*, size_t)> callback){
	int len = SSL_read(this->client_ssl[fd], data, static_cast<int>(data_length));
	int err = SSL_get_error(this->client_ssl[fd], len);
	switch(err){
	case SSL_ERROR_NONE:
		break;
	case SSL_ERROR_WANT_READ:
		PRINT("WANT READ")
		// Nothing to read, nonblocking mode.
		return 0;
	case SSL_ERROR_ZERO_RETURN:
		ERROR("server read zero")
		return -2;
	case SSL_ERROR_SYSCALL:
		perror("SSL_ERROR_SYSCALL");
		ERR_print_errors_fp(stdout);
		return -3;
	default:
		ERROR("other SSL_read " << err)
		ERR_print_errors_fp(stdout);
		return -1;
	}
	data[len] = 0;
	return callback(fd, data, static_cast<size_t>(len));
}

/**
 * @brief Importantly implements @see EpollServer::accept_continuation.
 *
 * See EpollServer::accept_continuation
 *
 * Completed the SSL handshake as defined by the SSL context.
 * If the SSL handshake fails, the the client did *not* successfully connect and is dumped.
 */
bool TlsEpollServer::accept_continuation(int* new_client_fd){
	if((this->client_ssl[*new_client_fd] = SSL_new(this->ctx)) == 0){
		ERROR("SSL_new")
		ERR_print_errors_fp(stdout);
		return true;
	}

	SSL_set_fd(this->client_ssl[*new_client_fd], *new_client_fd);

//	SSL_accept fails if socket is non_blocking.
	if(SSL_accept(this->client_ssl[*new_client_fd]) <= 0){
		ERROR("SSL_accept")
		ERR_print_errors_fp(stdout);
		return true;
	}

	return false;
}

/// Simply cleans up the OpenSSL context.
TlsEpollServer::~TlsEpollServer(){
	SSL_CTX_free(this->ctx);
	EVP_cleanup();
}
