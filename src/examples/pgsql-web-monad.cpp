#include "symmetric-tcp-client.hpp"
#include "web-monad.hpp"

int main(int argc, char **argv){
	std::string public_directory;
	std::string hostname;
	std::string ssl_certificate;
	std::string ssl_private_key;
	std::string database_ip = "127.0.0.1";
	std::string keyfile;

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("database_ip", database_ip, {"-dbip"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::parse_arguments(argc, argv, "This is a web monad which holds the routes for bwackwat.com; gets data from a secure PostgreSQL provider.");

	SymmetricTcpClient provider(database_ip, 10000, keyfile);

	WebMonad monad(hostname, public_directory, ssl_certificate, ssl_private_key);

	monad.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API!\"}";
	});

	monad.route("GET", "/routes", [&](JsonObject*)->std::string{
		return monad.routes_string;
	});

	std::string get_users = "{\"table\":\"users\",\"operation\":\"all\"}";
	monad.route("GET", "/users", [&](JsonObject*)->std::string{
		return provider.communicate(get_users.c_str(), get_users.length());
	});

	std::string get_user = "{\"table\":\"users\",\"operation\":\"where\"}";
	monad.route("GET", "/user", [&](JsonObject*)->std::string{
		return provider.communicate(get_users.c_str(), get_users.length());
	});

	monad.start();
}
