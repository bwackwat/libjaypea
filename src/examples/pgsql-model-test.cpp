#include <vector>
#include <string>

#include "util.hpp"
#include "pgsql-model.hpp"
#include "symmetric-event-server.hpp"

static JsonObject* json_error(std::string message){
	JsonObject* error = new JsonObject(OBJECT);
	error->objectValues["error"] = new JsonObject(message);
	return error;
}

int main(int argc, char** argv){
	std::string connection_string;
	std::string keyfile = "etc/keyfile";

	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("postgresql_connection", connection_string, {"-pcs"});
	Util::parse_arguments(argc, argv, "This is a securely communicating, database abstraction layer.");

	PgSqlModel* users = new PgSqlModel(connection_string,
		"users",
		{"id", "username", "password", "email", "first_name", "last_name", "created_on"});
	PgSqlModel* poi = new PgSqlModel(connection_string,
		"poi",
		{"id", "owner_id", "label", "description", "location", "created_on"});
	PgSqlModel* posts = new PgSqlModel(connection_string,
		"posts",
		{"id", "owner_id", "title", "content", "created_on"});

	std::unordered_map<std::string, PgSqlModel*> db_tables = {
		{users->table, users},
		{poi->table, poi},
		{posts->table, posts}
/*		"users", new PgSqlModel(connection_string, "users",
			{"id", "username", "password", "email", "first_name", "last_name", "created_on"}),
		"poi", new PgSqlModel(connection_string, "poi",
			{"id", "owner_id", "label", "description", "location", "created_on"}),
		"posts", new PgSqlModel(connection_string, "posts",
			{"id", "owner_id", "title", "content", "created_on"})*/
	};
	
	SymmetricEventServer server(keyfile, 10000, 1);

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		JsonObject* request = new JsonObject();
		request->parse(data);
		JsonObject* response;

		if(request->objectValues.count("table") &&
		request->objectValues.count("operation")){
			std::string& table = request->objectValues["table"]->stringValue;
			std::string& operation = request->objectValues["operation"]->stringValue;
			if(operation == "where"){
				if(request->objectValues.count("key") &&
				request->objectValues.count("value")){
					response = db_tables[table]->Where(request->objectValues["key"]->stringValue, request->objectValues["value"]->stringValue);
				}else{
					response = json_error("Missing \"key\" and/or \"value\" JSON.");
				}
			}else if(operation == "insert"){
				if(request->objectValues.count("key") &&
				request->objectValues.count("value")){
					response = db_tables[table]->Where(request->objectValues["key"]->stringValue, request->objectValues["value"]->stringValue);
				}else{
					response = json_error("Missing \"key\" and/or \"value\" JSON.");
				}
			}else{
				response = json_error("Invalid \"operation\" " + operation + ".");
			}
		}else{
			response = json_error("Missing \"table\" and/or \"operation\" JSON.");
		}

		delete request;
		std::string response_string = response->stringify();
		delete response;

		if(server.send(fd, response_string.c_str(), response_string.length()) < 0){
			ERROR("server.send")
			return -1;
		}

		return data_length;
	};

	// Easy DDOS protection. (This is not a multi-user tool.)
	server.on_disconnect = [&](int){
		sleep(10);
	};

	server.run(false, 1);
}

//"host=localhost dbname=webservice user=postgres password=aq12ws"
