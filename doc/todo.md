# TODO

(This list is possibly up-to-date.)

1. Refactor EpollServer::close_client. It does NOT require the index parameter. Can the lambda within run_thread be refactored?
2. Implement an extension of the PrivateEpollServer, the WebSocketServer. (See WIP example src/examples/wss-server.cpp.)
3. EpollServer events! This is a tricky multithreading problem, but is critical for many systems I interested in. Basically a broadcasting mechanism to write to all fds connected to a server.
4. Symmetric servers need to try and decrypt particular lengths of received data. PACKET_LIMIT will not work if two TCP packets get merged together. Have the first four bytes represent the length of the expected encrypted data -- this does not necessarily pose a security threat especially since random salts and tansaction numbers remain within the encrypted data.
5. Write some Doxygen documentation for critical classes.
6. FIND PLACES WHERE STD::STRING OR STL CLASSES DON'T NEED TO BE COPIED/COPY-CONSTRUCTED!
7. Add timeout parameter to EpollServer.
8. Created a "DistributedServer" class which manages a list of hosts, does heartbeats, and negotiates master nodes. Hosts connected this way should be in the same data center--more research is required here.
9. PrivateEventClient (openssl client) implementing TcpEventClient.
10. Refactor run_thread into classes or something crazy??? Ugh.
