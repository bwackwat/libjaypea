#include "http-api.hpp"
#include "pgsql-model.hpp"

int main(int argc, char **argv){
	std::string hostname;
	std::string admin;
	std::string public_directory;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int port = 443;
	int cache_megabytes = 30;
	std::string connection_string;
	
	/*
		Handle all the configurable arguments.
	*/

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("admin", hostname, {"-a"});
	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("cache_megabytes", &cache_megabytes,{"-cm"});
	Util::define_argument("postgresql_connection", connection_string, {"-pcs"});
	Util::parse_arguments(argc, argv, "This is an HTTP(S) JSON API which hold routes for jph2.net.");
	
	/*
		Initializes the HTTPS server, an encryptor, and an API.
	*/
	
	TlsEpollServer server(ssl_certificate, ssl_private_key, static_cast<uint16_t>(port), 10);
	SymmetricEncryptor encryptor;
	HttpApi api(public_directory, &server);
	api.set_file_cache_size(cache_megabytes);
	
	/*
		Setup database models.
	*/
	
	Column id("id", COL_AUTO);
	Column created_on("created_on", COL_AUTO);
	Column owner_id("owner_id");

	PgSqlModel* users = new PgSqlModel(connection_string, "users", {
		&id,
		new Column("username"),
		new Column("password", COL_HIDDEN),
		new Column("email"),
		new Column("first_name"),
		new Column("last_name"),
		&created_on
	}, ACCESS_ADMIN);
	
	PgSqlModel* poi = new PgSqlModel(connection_string, "poi", {
		&id,
		&owner_id,
		new Column("label"),
		new Column("description"),
		new Column("location"),
		&created_on
	}, ACCESS_USER);
	
	PgSqlModel* posts = new PgSqlModel(connection_string, "posts", {
		&id,
		&owner_id,
		new Column("title"),
		new Column("content"),
		&created_on
	}, ACCESS_USER);
	
	PgSqlModel* access = new PgSqlModel(connection_string, "access", {
		&id,
		&owner_id,
		new Column("title"),
		new Column("content"),
		&created_on
	}, ACCESS_USER);
	
	/*
		Start defining routes.
	*/
	
	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API V1!\",\n\"routes\":" + api.routes_string + "}";
	});
	
	api.route("GET", "/users", [&](JsonObject* json)->std::string{
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(json->GetStr("token")).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		return users->All()->stringify();
	}, {{"token", STRING}});
	api.route("POST", "/login", [&](JsonObject* json)->std::string{
		JsonObject* token = users->Access("username", json->GetStr("username"), Util::hash_value_argon2d(json->GetStr("password")));
		std::string response;
		if(token->objectValues.count("error")){
			response = token->stringify();
		}else{						
			response = "{\"id\":\"" + token->GetStr("id") + "\",\n\"token\":\"" + encryptor.encrypt(token->stringify()) + "\"}";
		}
		delete token;
		return response;
	}, {{"username", STRING}, {"password", STRING}}, std::chrono::seconds(1));

	/*api.route("POST", "/end", [&](JsonObject* json)->std::string{
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(request->objectValues["token"]->stringValue).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			response = PgSqlModel::Error("You cannot gain access to this.");
		}
		if(json->GetStr("token") != token){
			return std::string();
		}else{
			server->running = false;
			//server->send(server->broadcast_fd(), "Goodbye!");
			return "{\"result\":\"Goodbye!\"}";
		}
	}, {{"token", STRING}});*/

	/*
		USER


	api.route("GET", "/user", [&](JsonObject* json)->std::string{
		return provider.communicate(Where("users",
			"username",
			json->GetStr("username"),
			json->GetStr("token")));
	}, {{"username", STRING}, {"token", STRING}});
	
	api.route("POST", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Insert("users",
			json->objectValues["values"]));
	}, {{"values", ARRAY}}, std::chrono::hours(6)); // Can only register every 1 hours.
	
	api.route("PUT", "/user", [&](JsonObject* json)->std::string{	
		return provider.communicate(Update("users",
			json->GetStr("id"),
			json->objectValues["values"],
			json->GetStr("token")));
	}, {{"id", STRING}, {"values", OBJECT}, {"token", STRING}});
	

		BLOG

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

		POI
	*/
	/*

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
	*/

	api.start();
}
