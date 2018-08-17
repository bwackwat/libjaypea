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

	// Hell yeah, use ECDH.
	if(!SSL_CTX_set_ecdh_auto(this->ctx, 1)){
		ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_set_ecdh_auto");
	}

	// SSLv3 is insecure via poodles.
	SSL_CTX_set_options(this->ctx, SSL_OP_NO_TLSv1 | SSL_OP_NO_SSLv3 | SSL_OP_NO_SSLv2);

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
	DEBUG(this->name << ": SSL_free on " << *fd)
	SSL_free(this->client_ssl[*fd]);
	//delete this->client_ssl[*fd];
	this->client_ssl.erase(*fd);
	callback(fd);
}

/**
 * @brief Uses SSL_write instead of a regular write.
 *
 * See EpollServer::send
 */ 
bool TlsEpollServer::send(int fd, const char* data, size_t data_length){
	int len = 0, err = SSL_ERROR_WANT_WRITE;
	std::chrono::milliseconds send_start = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	std::chrono::milliseconds diff = std::chrono::milliseconds(0);
	std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

	while(err == SSL_ERROR_WANT_WRITE){
		if(diff > timeout){
			PRINT("SSL_write timeout...")
			return true;
		}

		len = SSL_write(this->client_ssl[fd], data, static_cast<int>(data_length));
		err = SSL_get_error(this->client_ssl[fd], len);

		diff = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()) - send_start;
	}
	switch(err){
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_WANT_WRITE:
			PRINT("WANT WRITE " << fd)
			// Nothing to read, nonblocking mode.
			return 0;
		case SSL_ERROR_ZERO_RETURN:
			ERROR("server write zero " << fd)
			return true;
		case SSL_ERROR_SYSCALL:
			PRINT(this->name << ": write SSL_ERROR_SYSCALL")
			ERR_print_errors_fp(stdout);
			return true;
		default:
			ERROR("other SSL_write " << err << " from " << fd)
			ERR_print_errors_fp(stdout);
			return true;
	}
	
	//DEBUG("SSL_write took milliseconds: " << diff.count())
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
	int len = 0, err = SSL_ERROR_WANT_READ;
	std::chrono::milliseconds recv_start = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	std::chrono::milliseconds diff = std::chrono::milliseconds(0);
	std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

	while(err == SSL_ERROR_WANT_READ){
		if(diff > timeout){
			PRINT("SSL_read timeout...")
			return true;
		}

		len = SSL_read(this->client_ssl[fd], data, static_cast<int>(data_length));
		err = SSL_get_error(this->client_ssl[fd], len);

		diff = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()) - recv_start;
	}
	switch(err){
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_WANT_READ:
			PRINT("WANT READ " << fd)
			// Nothing to read, nonblocking mode.
			return 0;
		case SSL_ERROR_ZERO_RETURN:
			ERROR("server read zero " << fd)
			return -2;
		case SSL_ERROR_SYSCALL:
			PRINT(this->name << ": read SSL_ERROR_SYSCALL")
			ERR_print_errors_fp(stdout);
			return -3;
		default:
			ERROR("other SSL_read " << err << " from " << fd)
			ERR_print_errors_fp(stdout);
			return -1;
	}
	
	//DEBUG("SSL_read took milliseconds: " << diff.count())
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
		ERROR("SSL_new " << *new_client_fd)
		ERR_print_errors_fp(stdout);
		return true;
	}

	if(SSL_set_fd(this->client_ssl[*new_client_fd], *new_client_fd) == 0){
		ERROR("SSL_set_fd" << *new_client_fd);
		ERR_print_errors_fp(stdout);
		return true;
	}
	
	DEBUG(this->name << ": SSL_new, done on " << *new_client_fd)

	int res, err = SSL_ERROR_WANT_READ;
	std::chrono::milliseconds accept_start = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	std::chrono::milliseconds diff = std::chrono::milliseconds(0);
	std::chrono::milliseconds timeout = std::chrono::milliseconds(10000);

	while(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE){
		if(diff > timeout){
			PRINT("SSL_accept timeout...")
			return true;
		}

		res = SSL_accept(this->client_ssl[*new_client_fd]);
		err = SSL_get_error(this->client_ssl[*new_client_fd], res);

		diff = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()) - accept_start;
	}
	if(err != SSL_ERROR_NONE){
		ERROR("SSL_accept " << *new_client_fd << ';' << err)
		return true;
	}
	DEBUG(this->name << ": SSL_accept took milliseconds: " << diff.count())

	return false;
}

/// Simply cleans up the OpenSSL context.
TlsEpollServer::~TlsEpollServer(){
	DEBUG("DELETE TLS EPOLL SERVER")
	for(auto iter = this->client_ssl.begin(); iter != this->client_ssl.end(); ++iter){
		SSL_free(iter->second);
		//delete iter->second;
		//this->client_ssl.erase(iter->first);
	}
	SSL_CTX_free(this->ctx);
	ERR_remove_state(0);
	ERR_free_strings();
	EVP_cleanup();
	//SSL_COMP_get_compression_methods();
	CRYPTO_cleanup_all_ex_data();
}


