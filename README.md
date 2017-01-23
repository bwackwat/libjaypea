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
```bash
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
```bash
bin/build-with-library.sh
> Usage: <relative path to cpp file> <executable name> <optional additional libraries/compiler flags>
bin/build-with-library.sh echo.cpp echo
```
4. Run your code:
```bash
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

1. [tcp-client](doc/tcp-client.md). You can speed test an TCP server.
2. [tcp-poll-server](doc/tcp-poll-server.md). A TCP server able to manage many connections using poll.
3. [modern-web-monad](doc/modern-web-monad.md). A modern web monad using SSL, JSON, and HTTP.
4. [ponal-client](doc/ponal.md). A simple command line client to ponald.
5. [ponal-server](doc/ponal.md). A simple redis-inspired key-value store server.
6. [com](doc/comd.md). An AES encrypted client for comd.
7. [comd](doc/comd.md). An AES encrypted alternative to SSH with file transfer support.
8. [read-stdin-tty](doc/comd.md). A tool to read from STDIN tty in raw mode.
9. [pgsql-model-test](doc/pgsql-model.md). A simple class to query a PostgreSQL database in a common way.
