#include <vector>
#include <string>

#include "util.hpp"
#include "pgsql-model.hpp"
#include "symmetric-event-server.hpp"

int main(int argc, char** argv){
	std::string keyfile;
	std::string connection_string;

	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("postgresql_connection", connection_string, {"-pcs"});
	Util::parse_arguments(argc, argv, "This is a securely communicating, database abstraction layer.");

	// Reusable, common columns.
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
	});
	PgSqlModel* poi = new PgSqlModel(connection_string, "poi", {
		&id,
		&owner_id,
		new Column("label"),
		new Column("description"),
		new Column("location"),
		&created_on
	});
	PgSqlModel* posts = new PgSqlModel(connection_string, "posts", {
		&id,
		&owner_id,
		new Column("title"),
		new Column("content"),
		&created_on
	});

	std::unordered_map<std::string, PgSqlModel*> db_tables = {
		{users->table, users},
		{poi->table, poi},
		{posts->table, posts}
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
			if(operation == "all"){
				response = db_tables[table]->All();
			}else if(operation == "where"){
				if(request->objectValues.count("key") &&
           		request->objectValues["key"]->type == STRING &&
				request->objectValues.count("value") &&
           		request->objectValues["value"]->type == STRING){
					response = db_tables[table]->Where(request->objectValues["key"]->stringValue, request->objectValues["value"]->stringValue);
				}else{
					response = PgSqlModel::Error("Missing \"key\" and/or \"value\" JSON strings.");
				}
			}else if(operation == "insert"){
				if(request->objectValues.count("values") &&
				request->objectValues["values"]->type == ARRAY){
					response = db_tables[table]->Insert(request->objectValues["values"]->arrayValues);
				}else{
					response = PgSqlModel::Error("Missing \"values\" JSON array.");
				}
			}else if(operation == "update"){
				if(request->objectValues.count("id") &&
           		request->objectValues["id"]->type == STRING &&
				request->objectValues.count("values") &&
				request->objectValues["values"]->type == OBJECT){
					response = db_tables[table]->Update(request->objectValues["id"]->stringValue, request->objectValues["values"]->objectValues);
				}else{
					response = PgSqlModel::Error("Missing \"values\" JSON object.");
				}
			}else{
				response = PgSqlModel::Error("Invalid \"operation\" " + operation + ".");
			}
		}else{
			response = PgSqlModel::Error("Missing \"table\" and/or \"operation\" JSON strings.");
		}

		delete request;
		std::string response_string = response->stringify();
		delete response;

		if(server.send(fd, response_string.c_str(), response_string.length())){
			ERROR("server.send")
			return -1;
		}

		return data_length;
	};

	// Easy DDOS protection. (This is not a multi-user tool.)
	// server.on_disconnect = [&](int){
	//	sleep(10);
	// };

	server.run(false, 1);
}
