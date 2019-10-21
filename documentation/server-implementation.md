# Simple Server Implementation Example

* Execute some BASH commands to download and build the library:
```
git clone https://github.com/bwackwat/libjaypea
cd libjaypea
./scripts/setup-centos7.sh
./scripts/build-library.sh
```
* Implement the library e.g. a simple echo server (echo.cpp):
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
* Build your code:
```
./scripts/build-with-library.sh
> Usage: <relative path to cpp file> <executable name> <optional additional libraries/compiler flags>
./scripts/build-with-library.sh echo.cpp echo
```
* Run your code:
```
./artifacts/echo
```

## Example Explanation

The example above creates a multithreaded tcp epoll server listening on port 10000, with a maximum of 10 open sockets per thread.

It is a basic echo server.

```server.on_read``` is the definitive function that will be called every time a fd (socket connection) has provided data to read.

```server.send``` will return true on error. If it fails, the lambda returns -1; this will close the connection for safety purposes.

If the send succeeds, then the function returns with a success code (the given length).

```server.run``` will start the server with a number of threads usually equal to the number of available hyperthreads (std::thread::hardware_concurrency()).
