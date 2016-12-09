# libjaypea

Too many C++ projects became a library. Currently a set of similarly designed networking programs.

## Projects and Programs (with docs)

1. [tcp-client](doc/tcp-client.md). You can speed test an TCP server.
2. [tcp-poll-server](doc/tcp-poll-server.md). A TCP server able to manage many connections using poll.
3. [modern-web-monad](doc/modern-web-monad.md). A modern web monad using SSL, JSON, and HTTP.
4. [ponal-client](doc/ponal.md). A simple command line client to ponald.
5. [ponal-server](doc/ponal.md). A simple redis-inspired key-value store server.
6. [com](doc/comd.md). An AES encrypted client for comd.
7. [comd](doc/comd.md). An AES encrypted alternative to SSH with file transfer support.
8. [read-stdin-tty](doc/comd.md). A tool to read from STDIN tty in raw mode.
9. [pgsql-model-test](doc/pgsql-model.md). A simple class to query a PostgreSQL database in a common way.

## Notes

* Developed and tested on CentOS 7. ```bin/build.sh``` just uses clang++...
* This is NOT YET a header-only library, but soon all programs will compile only as examples.
* ```etc/``` contains files for these programs that might be shared.
* ```bin/build.sh``` builds all examples/programs.
