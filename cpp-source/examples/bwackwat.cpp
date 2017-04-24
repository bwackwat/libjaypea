#include "symmetric-tcp-client.hpp"
#include "http-api.hpp"

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
	int port = 443;
	int cache_megabytes = 30;
	bool http = false;

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("database_ip", database_ip, {"-dbip"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("port", &port);
	Util::define_argument("http", &http);
	Util::define_argument("cache_megabytes", &cache_megabytes,{"-cm"});
	Util::parse_arguments(argc, argv, "This is an HTTP(S) JSON API which hold routes for bwackwat.com by interfacing with a PostgreSQL provider.");

	SymmetricTcpClient provider(database_ip, 20000, keyfile);

	EpollServer* server;
	if(http){
		server = new EpollServer(static_cast<uint16_t>(port), 10);
	}else{
		server = new TlsEpollServer(ssl_certificate, ssl_private_key, static_cast<uint16_t>(port), 10);
	}

	HttpApi api(public_directory, server);
	
	api.set_file_cache_size(cache_megabytes);

	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API V1!\",\n\"routes\":" + api.routes_string + "}";
	});

	/*
		USER
	*/

	api.route("GET", "/users", [&](JsonObject* json)->std::string{
		return provider.communicate(All("users",
			json->GetStr("token")));
	}, {{"token", STRING}});

	api.route("GET", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Where("users",
			"username",
			json->GetStr("username"),
			json->GetStr("token")));
	}, {{"username", STRING}, {"token", STRING}});
	
	api.route("POST", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Insert("users",
			json->objectValues["values"]));
	}, {{"values", ARRAY}}, std::chrono::hours(6)); // Can only register every 6 hours.
	
	api.route("PUT", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Update("users",
			json->GetStr("id"),
			json->objectValues["values"],
			json->GetStr("token")));
	}, {{"id", STRING}, {"values", OBJECT}, {"token", STRING}});
	
	api.route("POST", "/login", [&](JsonObject* json)->std::string{
		return provider.communicate(Login("users",
			json->GetStr("username"),
			json->GetStr("password")));
	}, {{"username", STRING}, {"password", STRING}}, std::chrono::seconds(1));

	/*
		BLOG
	*/

	api.route("GET", "/blog", [&](JsonObject* json)->std::string{
		return provider.communicate(Where("posts", "username", json->GetStr("username")));
	}, {{"username", STRING}});

	api.route("POST", "/blog", [&](JsonObject* json)->std::string{
		return provider.communicate(Insert("posts",
			json->objectValues["values"],
			json->GetStr("token")));
	}, {{"values", ARRAY}, {"token", STRING}});

	api.route("PUT", "/blog", [&](JsonObject* json)->std::string{
		return provider.communicate(Update("posts",
			json->GetStr("id"),
			json->objectValues["values"],
			json->GetStr("token")));
	}, {{"id", STRING}, {"values", OBJECT}, {"token", STRING}});

	/*
		POI
	*/

	api.route("GET", "/poi", [&](JsonObject* json)->std::string{
		return provider.communicate(Where("poi",
			json->GetStr("key"),
			json->GetStr("value")));
	}, {{"key", STRING}, {"value", STRING}});

	api.route("POST", "/poi", [&](JsonObject* json)->std::string{
		return provider.communicate(Insert("poi",
			json->objectValues["values"],
			json->GetStr("token")));
	}, {{"values", ARRAY}, {"token", STRING}});

	api.route("PUT", "/poi", [&](JsonObject* json)->std::string{
		return provider.communicate(Update("poi",
			json->GetStr("id"),
			json->objectValues["values"],
			json->GetStr("token")));
	}, {{"id", STRING}, {"values", OBJECT}, {"token", STRING}});

	api.start();
}
