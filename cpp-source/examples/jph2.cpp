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
	Util::define_argument("https_port", &port, {"-p"});
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
	Column owner_id("owner_id");
	Column deleted("deleted", COL_AUTO);
	Column created("created", COL_AUTO);
	Column modified("modified", COL_AUTO);

	PgSqlModel* users = new PgSqlModel(connection_string, "users", {
		&id,
		new Column("username"),
		new Column("password", COL_HIDDEN),
		new Column("primary_token"),
		new Column("secondary_token"),
		new Column("email"),
		new Column("first_name"),
		new Column("last_name"),
		&deleted,
		&modified,
		&created
	}, ACCESS_ADMIN);
	
	PgSqlModel* poi = new PgSqlModel(connection_string, "poi", {
		&id,
		&owner_id,
		new Column("label"),
		new Column("description"),
		new Column("location"),
		&deleted,
		&modified,
		&created
	}, ACCESS_USER);
	
	PgSqlModel* threads = new PgSqlModel(connection_string, "threads", {
		&id,
		&owner_id,
		new Column("name"),
		new Column("description"),
		&deleted,
		&modified,
		&created
	}, ACCESS_USER);
	
	PgSqlModel* messages = new PgSqlModel(connection_string, "messages", {
		&id,
		&owner_id,
		new Column("thread_id"),
		new Column("title"),
		new Column("content"),
		&deleted,
		&modified,
		&created
	}, ACCESS_USER);
	
	PgSqlModel* access_types = new PgSqlModel(connection_string, "access_types", {
		&id,
		new Column("name"),
		new Column("description"),
		&deleted,
		&modified,
		&created
	}, ACCESS_USER);
	
	PgSqlModel* access = new PgSqlModel(connection_string, "access", {
		&owner_id,
		new Column("access_type_id")
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
			token.parse(encryptor.decrypt(JsonObject::deescape(json->GetStr("token"))).c_str());
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
			response = "{\"id\":\"" + token->GetStr("id") + "\",\"token\":" + JsonObject::escape(encryptor.encrypt(token->stringify())) + "}";
		}
		delete token;
		return response;
	}, {{"username", STRING}, {"password", STRING}}, std::chrono::seconds(1));
	
	api.route("POST", "/token", [&](JsonObject* json)->std::string{
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(JsonObject::deescape(json->GetStr("token"))).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		return "{\"result\":\"Token is good\"}";
	}, {{"token", STRING}});
	
	api.route("POST", "/user", [&](JsonObject* json)->std::string{
		std::string response;
		
		JsonObject* temp_users = users->All();
		
		if(json->objectValues["values"]->arrayValues.size() > 1){
			json->objectValues["values"]->arrayValues[1]->stringValue =
				Util::hash_value_argon2d(json->objectValues["values"]->arrayValues[1]->stringValue);
		}
		
		JsonObject* newuser = users->Insert(json->objectValues["values"]->arrayValues);
		
		if(newuser->objectValues.count("error") == 0){
			if(temp_users->arrayValues.size() == 0){
				access->Insert(std::vector<JsonObject*> {
					new JsonObject(newuser->GetStr("id")),
					new JsonObject("0")});
			}
		}
		
		response = newuser->stringify(false);
		delete temp_users;
		delete newuser;
		return response;
	}, {{"values", ARRAY}}, std::chrono::hours(1)); // Can only register every 1 hours.
	
	api.route("POST", "/thread", [&](JsonObject* json)->std::string{
		std::string response;
		
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(JsonObject::deescape(json->GetStr("token"))).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		
		json->objectValues["values"]->arrayValues.insert(
			json->objectValues["values"]->arrayValues.begin(), token.objectValues["id"]);
		
		JsonObject* newthread = threads->Insert(json->objectValues["values"]->arrayValues);
		
		response = newthread->stringify(false);
		delete newthread;
		return response;
	}, {{"values", ARRAY}, {"token", STRING}});
	
	api.route("PUT", "/thread", [&](JsonObject* json)->std::string{
		std::string response;
		
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(JsonObject::deescape(json->GetStr("token"))).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		
		JsonObject* newthread = threads->Update(json->GetStr("id"), json->objectValues["values"]->objectValues);
		
		response = newthread->stringify(false);
		delete newthread;
		return response;
	}, {{"values", OBJECT}, {"id", STRING}, {"token", STRING}});
	
	api.route("DELETE", "/thread", [&](JsonObject* json)->std::string{
		std::string response;
		
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(JsonObject::deescape(json->GetStr("token"))).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		
		JsonObject* newthread = threads->Delete(json->GetStr("id"));
		
		response = newthread->stringify(false);
		delete newthread;
		return response;
	}, {{"id", STRING}, {"token", STRING}});
	
	api.route("POST", "/get/user/threads", [&](JsonObject* json)->std::string{
		std::string response;
		
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(JsonObject::deescape(json->GetStr("token"))).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		
		JsonObject* temp_threads = threads->Where("owner_id", token.GetStr("id"));
		
		response = temp_threads->stringify(false);
		delete temp_threads;
		return response;
	}, {{"token", STRING}});
	
	api.route("POST", "/get/user/thread", [&](JsonObject* json)->std::string{
		std::string response;
		
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(JsonObject::deescape(json->GetStr("token"))).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		
		JsonObject* temp_threads;
		try{
			temp_threads = threads->Where("id", json->GetStr("id"));
		
			if(temp_threads->arrayValues.size() == 0){
				delete temp_threads;
				return "{\"error\":\"That thread doesn't exist!\"}";
			}
			
			if(temp_threads->arrayValues[0]->GetStr("owner_id") != token.GetStr("id")){
				delete temp_threads;
				return "{\"error\":\"You cannot gain access to this!\"}";
			}
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		
		response = temp_threads->arrayValues[0]->stringify(false);
		delete temp_threads;
		return response;
	}, {{"token", STRING}, {"id", STRING}});
	
	api.route("POST", "/get/thread/messages", [&](JsonObject* json)->std::string{
		std::string response;
		
		JsonObject token;
		try{
			token.parse(encryptor.decrypt(JsonObject::deescape(json->GetStr("token"))).c_str());
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		
		try{
			JsonObject* temp_threads = threads->Where("id", json->GetStr("id"));
			
			if(temp_threads->arrayValues.size() == 0){
				delete temp_threads;
				return "{\"error\":\"That thread doesn't exist!\"}";
			}
			
			if(temp_threads->arrayValues[0]->GetStr("owner_id") != token.GetStr("id")){
				delete temp_threads;
				return "{\"error\":\"You cannot gain access to this!\"}";
			}
			delete temp_threads;
		
			JsonObject* temp_messages = messages->Where("thread_id", json->GetStr("id"));
			response = temp_messages->stringify(false);
			delete temp_messages;
		}catch(const std::exception& e){
			PRINT(e.what())
			return "{\"error\":\"You cannot gain access to this!\"}";
		}
		return response;
	}, {{"token", STRING}, {"id", STRING}});

	api.start();
}
