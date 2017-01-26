#include <csignal>

#include "util.hpp"
#include "private-tcp-server.hpp"

PrivateEpollServer::PrivateEpollServer(std::string certificate, std::string private_key, uint16_t port, size_t max_connections)
:EpollServer(port, max_connections, "PrivateEpollServer"){
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
        if(SSL_CTX_use_certificate_file(this->ctx, certificate.c_str(), SSL_FILETYPE_PEM) != 1){
                ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_use_certificate_file");
        }
        if(SSL_CTX_use_PrivateKey_file(this->ctx, private_key.c_str(), SSL_FILETYPE_PEM) != 1){
                ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_use_PrivateKey_file");
        }

	std::signal(SIGPIPE, SIG_IGN);

	SSL_CTX_set_mode(this->ctx, SSL_OP_NO_SSLv3);

	if(SSL_CTX_set_cipher_list(this->ctx, "ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS") == 0){
                ERR_print_errors_fp(stdout);
		throw std::runtime_error(this->name + "SSL_CTX_set_cipher_list");
	}

//	PRINT("SSL Mode: " << SSL_CTX_set_mode(this->ctx, SSL_MODE_ENABLE_PARTIAL_WRITE))
}

// Basically does an SSL_free before closing the socket.
void PrivateEpollServer::close_client(size_t index, int* fd, std::function<void(size_t, int*)> callback){
	SSL_free(this->client_ssl[*fd]);
	callback(index, fd);
}

bool PrivateEpollServer::send(int fd, const char* data, size_t data_length){
	int len = SSL_write(this->client_ssl[fd], data, static_cast<int>(data_length));
	switch(SSL_get_error(this->client_ssl[fd], len)){
	case SSL_ERROR_NONE:
		break;
	default:
		ERROR("SSL_write")
		return true;
	}
	if(len != static_cast<int>(data_length)){
		ERROR("Invalid number of bytes written...")
	}
	this->write_counter[fd]++;
	return false;
}

ssize_t PrivateEpollServer::recv(int fd, char* data, size_t data_length){
	int len;
	len = SSL_read(this->client_ssl[fd], data, static_cast<int>(data_length));
	switch(SSL_get_error(this->client_ssl[fd], len)){
	case SSL_ERROR_NONE:
		break;
	case SSL_ERROR_WANT_READ:
		// Nothing to read, nonblocking mode.
		return 0;
	case SSL_ERROR_ZERO_RETURN:
		ERROR("server read zero")
		return -2;
	default:
		ERROR("other SSL_read")
                ERR_print_errors_fp(stdout);
		return -1;
	}
	data[len] = 0;
	return this->on_read(fd, data, static_cast<ssize_t>(len));
}

bool PrivateEpollServer::accept_continuation(int* new_client_fd){
	if((this->client_ssl[*new_client_fd] = SSL_new(this->ctx)) == 0){
		ERROR("SSL_new")
		ERR_print_errors_fp(stdout);
		*new_client_fd = -1;
		return true;
	}

	SSL_set_fd(this->client_ssl[*new_client_fd], *new_client_fd);

//	SSL_accept fails if socket is non_blocking.
	if(SSL_accept(this->client_ssl[*new_client_fd]) <= 0){
		ERROR("SSL_accept")
		ERR_print_errors_fp(stdout);
		*new_client_fd = -1;
		return true;
	}

	return false;
}

PrivateEpollServer::~PrivateEpollServer(){
	SSL_CTX_free(this->ctx);
	EVP_cleanup();
}
