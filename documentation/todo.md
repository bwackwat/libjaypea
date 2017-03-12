# TODO

## 3/12/2017

1. Use const for unchanging methods.
2. Use PVS Studio for static analysis.
3. Create a pure interface for the TCP server/endpoint.
4. Verify UTF-8 works across JPON usage.
5. Setup API rate limiting via IP tracking.
6. Record code activity for statistical purposes. Record in DB.
7. Create base DB interface, backup to simple C++ DB.
8. Use separate key for symmetric encyption HMAC.
9. Dump distributed server code.
10. Create integration tests for continuously integrating code. Libjaypea becomes a build server by default?

## 2/4/2017

1. Comd block-size based reading doesn't read newlines? Something is up.
2. POI Javascript is totally deprecated. Needs to use OpenLayers 3.
3. Use setsockopt to add a timeout for sockets? This would be a great alternative to timerfds in epoll.
4. WebSocket Server. (Totally not working.)
5. Generate Doxygen HTML into affable-escapade.
6. FIND PLACES WHERE STD::STRING OR STL CLASSES DON'T NEED TO BE COPIED/COPY-CONSTRUCTED!
7. Created a "DistributedServer" class which manages a list of hosts, does heartbeats, and negotiates master nodes. Hosts connected this way should be in the same data center--more research is required here.
8. PrivateEventClient (openssl client) implementing TcpEventClient.
9. Refactor run_thread into classes or something crazy??? Ugh.
10. Docker to orchestrate libjaypea servers?

## 1/21/2017

1. Refactor EpollServer::close_client. It does NOT require the index parameter. Can the lambda within run_thread be refactored?
2. Implement an extension of the PrivateEpollServer, the WebSocketServer. (See WIP example src/examples/wss-server.cpp.)
3. EpollServer events! This is a tricky multithreading problem, but is critical for many systems this library was intended to be suited for. Basically a broadcasting mechanism to write to all fds connected to a server.
4. Symmetric servers need to try and decrypt particular lengths of received data. PACKET_LIMIT will not work if two TCP packets get merged together. Have the first four bytes represent the length of the expected encrypted data -- this does not necessarily pose a security threat especially since random salts and tansaction numbers remain within the encrypted data.
5. Write some Doxygen documentation for critical classes.
6. FIND PLACES WHERE STD::STRING OR STL CLASSES DON'T NEED TO BE COPIED/COPY-CONSTRUCTED!
7. Add timeout parameter to EpollServer.
8. Created a "DistributedServer" class which manages a list of hosts, does heartbeats, and negotiates master nodes. Hosts connected this way should be in the same data center--more research is required here.
9. PrivateEventClient (openssl client) implementing TcpEventClient.
10. Refactor run_thread into classes or something crazy??? Ugh.
