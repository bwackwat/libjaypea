#include "web-monad.hpp"

int main(int argc, char **argv){
	std::string public_directory;
	std::string hostname;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int http_port = 80;
	int https_port = 443;

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("http", &http_port);
	Util::define_argument("https", &https_port);
	Util::parse_arguments(argc, argv, "This is a modern web server monad which starts an HTTP redirection server, an HTTPS server for files, and a JSON API. Configured via etc/configuration.json.");

	WebMonad monad(hostname, public_directory, ssl_certificate, ssl_private_key,
		static_cast<uint16_t>(http_port), static_cast<uint16_t>(https_port));

	monad.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API!\"}";
	});

	monad.route("GET", "/routes", [&](JsonObject*)->std::string{
		return monad.routes_string;
	});

	monad.start();
}
