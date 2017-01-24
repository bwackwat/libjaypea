# libjaypea

A C++11 networking library with examples.

Limited to "Linux" environments. Scripts are tested and used on CentOS 7.

Uses:
* Lots of C.
* CryptoPP for symmetric key encryption.
* OpenSSL for private key encryption.
* libpqxx for PostgreSQL integration.
* DigitalOcean API for quick deployments.

## Implementation Example

1. Execute some commands to load and build the library:
 ```
git clone https://github.com/bwackwat/libjaypea
cd libjaypea
bin/setup-centos7.sh
bin/build-library.sh
```
2. Implement the library e.g. a simple echo server (echo.cpp):
 ```c++
#include "tcp-server.hpp"

int main(int, char**){
	EpollServer server(10000, 10);

	server.on_read = [&](int fd, const char* packet, size_t length)->ssize_t{
		if(server.send(fd, packet, length)){
			return -1;
		}
		return length;
	};

	server.run();

	return 0;
}
```
3. Build your code:
 ```
bin/build-with-library.sh
> Usage: <relative path to cpp file> <executable name> <optional additional libraries/compiler flags>
bin/build-with-library.sh echo.cpp echo
```
4. Run your code:
 ```
build/echo
```

### Example Explanation

The example above creates a multithreaded tcp epoll server listening on port 10000, with a maximum of 10 open sockets per thread.

It is a basic echo server.

```server.on_read``` is the definitive function that will be called every time a fd (socket connection) has provided data to read.

```server.send``` will return true on error. If it fails, the lambda returns -1; this will close the connection for safety purposes.

If the send succeeds, then the function returns with a success code (the given length).

```server.run``` will start the server with a number of threads usually equal to the number of available hyperthreads (std::thread::hardware_concurrency()).

## Provided Examples

In order of coolness.

1. [bwackwat](https://bwackwat.com/). Hopefully this example will replace NGINX running on bwackwat.com...
2. [comd/com](doc/comd.md). An AES encrypted server and client. The server provides access to a shell, the client sends commands to that remote shell. Like sshd/ssh.
3. [message-api](doc/modern-web-monad.md). An HTTPS JSON API.
4. pgsql-provider. A secure database abstraction layer using pgsql-model as an ORM.
5. [pgsql-model-test](doc/pgsql-model.md). A simple class to query a PostgreSQL database in a common way.
6. [ponal-server/ponal-client](doc/ponal.md). A simple redis-inspired key-value store server and corresponding client.
7. [tcp-poll-server](doc/tcp-poll-server.md). A TCP server able to manage many connections using poll.
8. [tcp-client](doc/tcp-client.md). You can speed test an TCP server.
9. echo-server. A fast, simple echo server.
10. chat-server/chat-client. An incomplete chat server and client.
11. [read-stdin-tty](doc/comd.md). A tool to read from STDIN tty in raw mode.
12. keyfile-gen. Creates a key for comd/com.
13. http-redirecter. Redirects HTTP traffic. Good practice.
14. json-test. Tests JSON parsing.
15. queue-test. Tests the queue implementation.
