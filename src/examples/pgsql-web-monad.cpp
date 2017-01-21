#include "symmetric-tcp-client.hpp"
#include "web-monad.hpp"

static std::string All(const std::string& table){
	return "{\"table\":\"" + table + "\",\"operation\":\"all\"}";
}

static std::string Where(const std::string& table, const char* key, const char* value){
	return "{\"table\":\"" + table + "\",\"operation\":\"where\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
}

static std::string Insert(const std::string& table, JsonObject* values){
	return "{\"table\":\"" + table + "\",\"operation\":\"insert\",\"values\":" + values->stringify() + "}";
}

static std::string Update(const std::string& table, const char* id, JsonObject* values){
	return "{\"table\":\"" + table + "\",\"operation\":\"update\",\"id\":\"" + id + "\",\"values\":" + values->stringify() + "}";
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
		std::string query = All("users");
		
		return provider.communicate(query.c_str(), query.length());
	});

	monad.route("GET", "/user", [&](JsonObject* json)->std::string{
		std::string query = Where("users",
			json->objectValues["key"]->stringValue.c_str(),
			json->objectValues["value"]->stringValue.c_str());
	
		return provider.communicate(query.c_str(), query.length());
	}, {{"key", STRING}, {"value", STRING}});
	
	monad.route("POST", "/user", [&](JsonObject* json)->std::string{
		std::string query = Insert("users",
			json->objectValues["values"]);
	
		return provider.communicate(query.c_str(), query.length());
	}, {{"values", ARRAY}});
	
	monad.route("PUT", "/user", [&](JsonObject* json)->std::string{
		std::string query = Update("users",
			json->objectValues["id"]->stringValue.c_str(),
			json->objectValues["values"]);
	
		return provider.communicate(query.c_str(), query.length());
	}, {{"id", STRING}, {"values", OBJECT}});

	monad.start();
}
