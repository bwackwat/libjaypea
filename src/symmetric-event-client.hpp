#include "util.hpp"
#include "tcp-event-client.hpp"
#include "symmetric-encryptor.hpp"

class SymmetricEventClient : public EventClient{
private:
	SymmetricEncryptor encryptor;
public:
	SymmetricEventClient(std::string keyfile);

	bool send(int fd, const char* data, size_t data_length);
	ssize_t recv(int fd, char* data, size_t data_length);
};
