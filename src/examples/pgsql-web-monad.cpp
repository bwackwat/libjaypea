#include "symmetric-tcp-client.hpp"
#include "web-monad.hpp"

static std::string All(const std::string& table){
	return "{\"table\":\"" + table + "\",\"operation\":\"all\"}";
}

static std::string Where(const std::string& table, const std::string& key, const std::string& value){
	return "{\"table\":\"" + table + "\",\"operation\":\"where\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
}

static std::string Insert(const std::string& table, JsonObject* values){
	return "{\"table\":\"" + table + "\",\"operation\":\"insert\",\"values\":" + values->stringify() + "}";
}

static std::string Update(const std::string& table, const std::string& id, JsonObject* values){
	return "{\"table\":\"" + table + "\",\"operation\":\"update\",\"id\":\"" + id + "\",\"values\":" + values->stringify() + "}";
}

static std::string Access(const std::string& table, const std::string& key, const std::string& password){
	return "{\"table\":\"" + table + "\",\"operation\":\"access\",\"key\":\"" + key + "\",\"password\":" + password + "}";
}

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
		return "{\"result\":\"Welcome to the API!\",\n\"routes\":" + monad.routes_string + "}";
	});

	monad.route("GET", "/users", [&](JsonObject*)->std::string{
		return provider.communicate(All("users"));
	});

	monad.route("GET", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Where("users",
			json->objectValues["key"]->stringValue,
			json->objectValues["value"]->stringValue));
	}, {{"key", STRING}, {"value", STRING}});
	
	monad.route("POST", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Insert("users",
			json->objectValues["values"]));
	}, {{"values", ARRAY}});
	
	monad.route("PUT", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Update("users",
			json->objectValues["id"]->stringValue,
			json->objectValues["values"]));
	}, {{"id", STRING}, {"values", OBJECT}});
	
	monad.route("POST", "/login", [&](JsonObject* json)->std::string{
		return provider.communicate(Access("users",
			json->objectValues["username"]->stringValue,
			json->objectValues["password"]->stringValue));
	}, {{"username", STRING}, {"password", STRING}});

	monad.start();
}
