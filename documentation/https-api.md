# HttpsApi

An HTTPS server with a built-in JSON API for modern web applications in simple Linux environments.

## Features

* Requests are parsed and munged into JSON from:
  * A URL route, e.g. "/api/user".
  * A URL method, e.g. "POST".
  * A protocol, e.g. "HTTP/1.1".
  * HTTP headers, e.g. {"Content-Type":"application/json"}
  * URL Parameters, e.g. "/api/user?username=bwackwat&content=yay".
  * JSON in the request body.
* OpenSSL security.
* Multi-threaded epoll HTTPS server.
* ```configuration.json``` provides some easy configuration.
* JSON API routes have non-nestable required fields.
* HTTP Requests are safely read buffered across OpenSSL.
