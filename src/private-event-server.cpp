#include "util.hpp"
#include "tcp-event-server.hpp"
#include "private-event-server.hpp"

PrivateEventServer::PrivateEventServer(std::string certificate, std::string private_key, uint16_t port, size_t max_connections)
:EventServer("PrivateEventServer", port, max_connections){
	SSL_library_init();
	SSL_load_error_strings();

        if((this->ctx = SSL_CTX_new(SSLv23_server_method())) == 0){
                ERR_print_errors_fp(stdout);
		throw new std::runtime_error("SSL_CTX_new");
        }
        if(SSL_CTX_set_ecdh_auto(this->ctx, 1) == 0){
                ERR_print_errors_fp(stdout);
		throw new std::runtime_error("SSL_CTX_set_ecdh_auto");
	}
        if(SSL_CTX_use_certificate_file(this->ctx, certificate.c_str(), SSL_FILETYPE_PEM) != 1){
                ERR_print_errors_fp(stdout);
		throw new std::runtime_error("SSL_CTX_use_certificate_file");
        }
        if(SSL_CTX_use_PrivateKey_file(this->ctx, private_key.c_str(), SSL_FILETYPE_PEM) != 1){
                ERR_print_errors_fp(stdout);
		throw new std::runtime_error("SSL_CTX_use_PrivateKey_file");
        }

//	PRINT("SSL Mode: " << SSL_CTX_set_mode(this->ctx, SSL_MODE_ENABLE_PARTIAL_WRITE))
}

// Basically does an SSL_free before closing the socket.
void PrivateEventServer::close_client(size_t index, int fd, std::function<void(size_t, int)> callback){
	SSL_free(this->client_ssl[fd]);
	callback(index, fd);
}

bool PrivateEventServer::send(int fd, const char* data, size_t data_length){
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
		return true;
	}
	this->write_counter[fd]++;
	return false;
}

ssize_t PrivateEventServer::recv(int fd, char* data, size_t data_length){
	int len;
	len = SSL_read(this->client_ssl[fd], data, static_cast<int>(data_length));
	switch(SSL_get_error(this->client_ssl[fd], len)){
	case SSL_ERROR_NONE:
		break;
	case SSL_ERROR_WANT_READ:
		// Nothing to read, nonblocking mode.
		return false;
	case SSL_ERROR_ZERO_RETURN:
		ERROR("server read zero")
		return true;
	default:
		ERROR("other SSL_read")
		return true;
	}
	data[len] = 0;
	return this->on_read(fd, data, static_cast<size_t>(len));
}

bool PrivateEventServer::accept_continuation(int* new_client_fd){
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

	return EventServer::accept_continuation(new_client_fd);
}

PrivateEventServer::~PrivateEventServer(){
	SSL_CTX_free(this->ctx);
	EVP_cleanup();
}
