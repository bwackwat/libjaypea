#include "web-monad.hpp"

int main(int argc, char **argv){
	std::string hostname;
	std::string public_directory;
	std::string ssl_certificate = "etc/ssl/ssl.crt";
	std::string ssl_private_key = "etc/ssl/ssl.key";

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::parse_arguments(argc, argv, "This is a modern web server monad which starts an HTTP redirection server, an HTTPS server for files, and a JSON API. Configured via etc/configuration.json.");

	WebMonad monad(hostname, public_directory, ssl_certificate, ssl_private_key);
	monad.route("/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API!\"}";
	});
	monad.route("/routes", [&](JsonObject*)->std::string{
		return monad.routes_string;
	});
	monad.start();
}
