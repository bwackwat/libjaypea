# modern-web-monad

An HTTPS server with a built-in JSON API for modern web applications in simple Linux environments.

## Features

* Forks an HTTP server which simply redirects all traffic to the HTTPS site.
* Requests are parsed for:
  * A URL route, e.g. "/api/user".
  * A URL method, e.g. "POST".
  * HTTP headers, e.g. {"Content-Type":"application/json"}
  * URL Parameters, e.g. "/api/user?username=bwackwat&content=yay". These parameters are merged with any JSON in the request body.
  * JSON in the request body.
* OpenSSL security.
* Multi-process epoll HTTPS server.
* ```configuration.json``` provides some easy configuration.
* JSON API routes have non-nestable required fields.
* HTTP Requests are safely read buffered across OpenSSL.

## TODO

* Buffer overflows? I doubt it ;)
* Flamegraph this thing.
