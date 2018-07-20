#include "http-api.hpp"
#include "pgsql-model.hpp"
#include "tls-websocket-server.hpp"


int main(int argc, char **argv){
	std::string hostname;
	std::string admin;
	std::string public_directory;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int http_port;
	int https_port;
	int chat_port;
	int cache_megabytes = 30;
	std::string connection_string;
	
	
	///////////////////////////////////////////////////////
	//	CONFIGURE ARGUMENTS
	///////////////////////////////////////////////////////
	

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("admin", hostname, {"-a"});
	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("http_port", &http_port, {"-p"});
	Util::define_argument("https_port", &https_port, {"-sp"});
	Util::define_argument("chat_port", &chat_port, {"-cp"});
	Util::define_argument("cache_megabytes", &cache_megabytes,{"-cm"});
	Util::define_argument("postgresql_connection", connection_string, {"-pcs"});
	Util::parse_arguments(argc, argv, "This is an HTTP(S) JSON API which hold routes for jph2.net.");
	
	
	///////////////////////////////////////////////////////
	//	INITIALIZE HTTPS SERVER AND API
	///////////////////////////////////////////////////////
	
	
	TlsEpollServer server(ssl_certificate, ssl_private_key, static_cast<uint16_t>(https_port), 10);
	SymmetricEncryptor encryptor;
	HttpApi api(public_directory, &server, &encryptor);
	api.set_file_cache_size(cache_megabytes);
	
	
	///////////////////////////////////////////////////////
	//	SETUP DATABASE MODELS
	///////////////////////////////////////////////////////
	
	
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
	
	
	///////////////////////////////////////////////////////
	//	START API ROUTES
	///////////////////////////////////////////////////////
	
	
	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API V1!\",\n\"routes\":" + api.routes_string + "}";
	});
	
	
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
	
	
	api.route("POST", "/token", [&](JsonObject*, JsonObject*)->std::string{
		return "{\"result\":\"Token is good\"}";
	});
	
	
	api.route("POST", "/get/users", [&](JsonObject*, JsonObject* token)->std::string{
		JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1"){
				return users->All()->stringify();
			}
		}
		
		return INSUFFICIENT_ACCESS;
	});
	
	
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
					new JsonObject("1")});
			}
		}
		
		response = newuser->stringify(false);
		delete temp_users;
		delete newuser;
		return response;
	}, {{"values", ARRAY}}, std::chrono::hours(1)); // Can only register every 1 hours.
	
	
	///////////////////////////////////////////////////////
	//	START OTHER ROUTES
	///////////////////////////////////////////////////////
	
	
	/*api.route("GET", "/my/access", [&](JsonObject*, JsonObject* token)->std::string{
		JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
		
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1"){
				has_access = true;
			}
		}
		JsonObject* accesses = access_types->Where("access_type_id", (*it)->GetStr("access_type"));
		
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1"){
				has_access = true;
			}
		}
		return threads->Where("owner_id", token->GetStr("id"))->stringify();
	});*/
	
	
	///////////////////////////////////////////////////////
	//	START THREAD ROUTES
	///////////////////////////////////////////////////////
	
	
	api.route("POST", "/my/threads", [&](JsonObject*, JsonObject* token)->std::string{
		return threads->Where("owner_id", token->GetStr("id"))->stringify();
	});
	
	
	api.route("GET", "/thread", [&](JsonObject* json)->std::string{
		return threads->Where("id", json->GetStr("id"))->stringify();
	}, {{"id", STRING}});
	
	
	api.route("POST", "/thread", [&](JsonObject* json, JsonObject* token)->std::string{
		bool has_access = false;
		JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1" ||
			(*it)->GetStr("access_type_id") == "2"){
				has_access = true;
			}
		}
		if(!has_access){
			return "{\"error\":\"This requires blogger access.\"}";
		}
		
		json->objectValues["values"]->arrayValues.insert(
			json->objectValues["values"]->arrayValues.begin(), token->objectValues["id"]);
		
		return threads->Insert(json->objectValues["values"]->arrayValues)->stringify();
	}, {{"values", ARRAY}});
	
	
	api.route("PUT", "/thread", [&](JsonObject* json, JsonObject* token)->std::string{	
		JsonObject* new_thread = threads->Where("id", json->GetStr("id"));
		
		if(new_thread->arrayValues.size() == 0){
			return "{\"error\":\"That thread doesn't exist!\"}";
		}
		
		if(new_thread->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
			return INSUFFICIENT_ACCESS;
		}
		
		return threads->Update(json->GetStr("id"), json->objectValues["values"]->objectValues)->stringify();
	}, {{"id", STRING}, {"values", OBJECT}});
	
	
	api.route("DELETE", "/thread", [&](JsonObject* json, JsonObject* token)->std::string{
		JsonObject* new_thread = threads->Where("id", json->GetStr("id"));
		
		if(new_thread->arrayValues.size() == 0){
			return "{\"error\":\"That thread doesn't exist!\"}";
		}
		
		if(new_thread->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
			return INSUFFICIENT_ACCESS;
		}
		
		return threads->Delete(json->GetStr("id"))->stringify(false);
	}, {{"id", STRING}});
	
	
	///////////////////////////////////////////////////////
	//	START MESSAGE ROUTES
	///////////////////////////////////////////////////////
	
	
	api.route("GET", "/thread/messages", [&](JsonObject* json)->std::string{
		return messages->Where("thread_id", json->GetStr("id"))->stringify();
	}, {{"id", STRING}});
	
	
	api.route("GET", "/message", [&](JsonObject* json)->std::string{
		return messages->Where("id", json->GetStr("id"))->stringify();
	}, {{"id", STRING}});
	
	
	api.route("GET", "/thread/messages/by/name", [&](JsonObject* json)->std::string{
		JsonObject* temp_threads = threads->Where("name", json->GetStr("name"));
		
		if(temp_threads->arrayValues.size() == 0){
			delete temp_threads;
			return "{\"error\":\"That thread doesn't exist!\"}";
		}
		
		return messages->Where("thread_id", temp_threads->arrayValues[0]->GetStr("id"))->stringify();
	}, {{"name", STRING}});
	
	
	api.route("POST", "/message", [&](JsonObject* json, JsonObject* token)->std::string{
		bool has_access = false;
		JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1" ||
			(*it)->GetStr("access_type_id") == "2"){
				has_access = true;
			}
		}
		if(!has_access){
			return "{\"error\":\"This requires blogger access.\"}";
		}
		
		
		json->objectValues["values"]->arrayValues.insert(
			json->objectValues["values"]->arrayValues.begin(), token->objectValues["id"]);
		
		return messages->Insert(json->objectValues["values"]->arrayValues)->stringify();
	}, {{"values", ARRAY}});
	
	
	api.route("PUT", "/message", [&](JsonObject* json, JsonObject* token)->std::string{
		JsonObject* new_message = messages->Where("id", json->GetStr("id"));
		
		if(new_message->arrayValues.size() == 0){
			return "{\"error\":\"That message doesn't exist!\"}";
		}
		
		if(new_message->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
			return INSUFFICIENT_ACCESS;
		}
		
		return messages->Update(json->GetStr("id"), json->objectValues["values"]->objectValues)->stringify();
	}, {{"id", STRING}, {"values", OBJECT}});
	
	
	api.route("DELETE", "/message", [&](JsonObject* json, JsonObject* token)->std::string{
		JsonObject* new_message = messages->Where("id", json->GetStr("id"));
		
		if(new_message->arrayValues.size() == 0){
			return "{\"error\":\"That message doesn't exist!\"}";
		}
		
		if(new_message->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
			return INSUFFICIENT_ACCESS;
		}
		
		return messages->Delete(json->GetStr("id"))->stringify();
	}, {{"id", STRING}});
	
	
	///////////////////////////////////////////////////////
	//	SETUP AND START HTTP REDIRECTER
	///////////////////////////////////////////////////////
	
	
	EpollServer http_server(static_cast<uint16_t>(http_port), 10);
	http_server.on_read = [&](int fd, const char* data, ssize_t)->ssize_t{
		const char* it = data;
		int state = 1;
		std::string url = "";
		url += hostname;

		for(; *it; ++it){
			if(state == 2){
				if(*it == '\r' || *it == '\n' || *it == ' '){
					break;
				}else{
					url += *it;
				}
			}else if(*it == ' '){
				state = 2;
			}
		}
		
		std::string response = Util::get_redirection_html(url);
		const char* http_response = response.c_str();
		size_t http_response_length = response.length();
		
		http_server.send(fd, http_response, http_response_length);
		return -1;
	};
	http_server.run(true, 1);
	
	
	///////////////////////////////////////////////////////
	//	SETUP AND START CHAT SERVER
	///////////////////////////////////////////////////////
	
	
	EpollServer* chat_server = new TlsWebsocketServer(ssl_certificate, ssl_private_key, static_cast<uint16_t>(chat_port), 10);
	chat_server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		JsonObject msg;
		msg.parse(data);

		JsonObject response(OBJECT);

		if(msg.HasObj("handle", STRING) &&
		msg.HasObj("message", STRING)){
			if(msg.GetStr("handle").length() < 5){
				response.objectValues["status"] = new JsonObject("Handle too short.");
			}else if(msg.GetStr("handle").length() > 16){
				response.objectValues["status"] = new JsonObject("Handle too long.");
			}else if(msg.GetStr("message").length() < 2){
				response.objectValues["status"] = new JsonObject("Message too short.");
			}else if(msg.GetStr("message").length() > 256){
				response.objectValues["status"] = new JsonObject("Message too long.");
			}else{
				response.objectValues["status"] = new JsonObject("Sent.");
				if(chat_server->EpollServer::send(chat_server->broadcast_fd(), data, static_cast<size_t>(data_length))){
					PRINT("broadcast fail")
					return -1;
				}
			}
		}else{
			response.objectValues["status"] = new JsonObject("Must provide handle and message.");
		}
		std::string sresponse = response.stringify(false);
		PRINT("SEND:" << sresponse)
		if(chat_server->send(fd, sresponse.c_str(), static_cast<size_t>(sresponse.length()))){
			PRINT("send prob")
			return -1;
		}
		return data_length;
	};
	chat_server->run(true, 1);
	

	api.start();
}

