# modern-web-monad

An HTTPS server with a built-in JSON API for modern web applications in simple Linux environments.

* Forks an HTTP server which simply redirects all traffic to the HTTPS site.
* Requests are parsed for:
  * A URL route, e.g. "/api/user".
  * URL Parameters, e.g. "/api/user?username=bwackwat&content=yay". These parameters are merged with any JSON in the request body.
  * JSON in the request body.
  * (HTTP methods are in-fact ignored.)
* Modern OpenSSL security.
* Single-process HTTPS server.
* ```configuration.json``` provides some easy configuration.
* JSON API routes have non-nestable required fields.
* HTTP Requests are safely read buffered across OpenSSL.

## TODO

* Create a setup script for CentOS 7 machines.
* Support Berkeley DB and/or PostgreSQL backend connectors.
* SSL_read timeout?
* Buffer overflows? I doubt it.
* Fork the HTTPS server depending on number of CPUs!
* Flamegraph this thing.
* Provide example frontend JavaScript usage of the API.
* Find out what kind of horrific C++ classes I can shove this code into. Probably not going to happen.
