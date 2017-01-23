#include "symmetric-tcp-client.hpp"
#include "https-api.hpp"

static std::string All(const std::string& table, const std::string& token = std::string()){
	return "{\"table\":\"" + table + "\",\"operation\":\"all\",\"token\":\"" + token + "\"}";
}

static std::string Where(const std::string& table, const std::string& key, const std::string& value, const std::string& token = std::string()){
	return "{\"table\":\"" + table + "\",\"operation\":\"where\",\"token\":\"" + token + "\",\"key\":\"" + key + "\",\"value\":\"" + value + "\"}";
}

static std::string Insert(const std::string& table, JsonObject* values, const std::string& token = std::string()){
	return "{\"table\":\"" + table + "\",\"operation\":\"insert\",\"token\":\"" + token + "\",\"values\":" + values->stringify() + "}";
}

static std::string Update(const std::string& table, const std::string& id, JsonObject* values, const std::string& token = std::string()){
	return "{\"table\":\"" + table + "\",\"operation\":\"update\",\"token\":\"" + token + "\",\"id\":\"" + id + "\",\"values\":" + values->stringify() + "}";
}

static std::string Login(const std::string& table, const std::string& username, const std::string& password){
	return "{\"table\":\"" + table + "\",\"operation\":\"login\",\"token\":\"\",\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
}

int main(int argc, char **argv){
	std::string public_directory;
	std::string ssl_certificate;
	std::string ssl_private_key;
	std::string database_ip = "127.0.0.1";
	std::string keyfile;
	int https_port = 443;

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("database_ip", database_ip, {"-dbip"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("https", &https_port);
	Util::parse_arguments(argc, argv, "This is an HTTPS JSON API which hold routes for bwackwat.com by interfacing with a PostgreSQL provider.");

	SymmetricTcpClient provider(database_ip, 10000, keyfile);

	HttpsApi server(public_directory, ssl_certificate, ssl_private_key, static_cast<uint16_t>(https_port));

	server.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API!\",\n\"routes\":" + server.routes_string + "}";
	});

	server.route("GET", "/users", [&](JsonObject* json)->std::string{
		return provider.communicate(All("users",
			json->GetStr("token")));
	}, {{"token", STRING}});

	server.route("GET", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Where("users",
			json->GetStr("key"),
			json->GetStr("value"),
			json->GetStr("token")));
	}, {{"key", STRING}, {"value", STRING}, {"token", STRING}});
	
	server.route("POST", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Insert("users",
			json->objectValues["values"]));
	}, {{"values", ARRAY}}, true);
	
	server.route("PUT", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Update("users",
			json->GetStr("id"),
			json->objectValues["values"],
			json->GetStr("token")));
	}, {{"id", STRING}, {"values", OBJECT}, {"token", STRING}});
	
	server.route("POST", "/login", [&](JsonObject* json)->std::string{
		return provider.communicate(Login("users",
			json->GetStr("username"),
			json->GetStr("password")));
	}, {{"username", STRING}, {"password", STRING}});

	server.start();
}
