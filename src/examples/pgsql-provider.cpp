#include <vector>
#include <string>

#include "argon2.h"

#include "util.hpp"
#include "pgsql-model.hpp"
#include "symmetric-event-server.hpp"

static std::string hash_value_argond2d(std::string password){
	const uint32_t t_cost = 5;
	const uint32_t m_cost = 1 << 16; //about 65MB
	const uint32_t parallelism = 1; //TODO: can use std::thread::hardware_concurrency();?

	std::vector<uint8_t> pwdvec(password.begin(), password.end());
	uint8_t* pwd = &pwdvec[0];
	const size_t pwdlen = password.length();

	//TODO: Should this be used??
	//INFO: If this changes, user passwords in DB will become invalid.
	std::string nonce = "itmyepicsalt!@12";
	std::vector<uint8_t> saltvec(nonce.begin(), nonce.end());
	uint8_t* salt = &saltvec[0];
	const size_t saltlen = nonce.length();

	uint8_t hash[128];
	
	char encoded[256];

	argon2_type type = Argon2_d;

	int res = argon2_hash(t_cost, m_cost, parallelism,
		pwd, pwdlen, salt, saltlen,
		hash, 128, encoded, 256, type, ARGON2_VERSION_NUMBER);
	
	if(res){
		ERROR("Hashing error: " << res)
		return std::string();
	}else{
		return std::string(encoded);
	}
}

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
	
	SymmetricEncryptor encryptor;

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
					response = PgSqlModel::Error("Missing \"id\" JSON string and/or \"values\" JSON object.");
				}
			}else if(operation == "access"){
				if(table == "users"){
					if(request->objectValues.count("username") &&
		      		request->objectValues["username"]->type == STRING &&
					request->objectValues.count("password") &&
		      		request->objectValues["password"]->type == STRING){
		      			std::string password = hash_value_argond2d(request->objectValues["password"]->stringValue);
		      			JsonObject* token = db_tables[table]->Access("username", request->objectValues["username"]->stringValue, password);
						if(token == 0){
							response = PgSqlModel::Error("Provided bad password.");
							delete token;
						}
						
						// This is nnecessary?
						// token->objectValues["verify"] = new JsonObject(hash_value_sha256(password.substr(password.length() / 2)));
						
						response = new JsonObject(OBJECT);
						response->objectValues["token"] = new JsonObject(encryptor.encrypt(token->stringify()));
						delete token;
					}else{
						response = PgSqlModel::Error("Missing \"username\" JSON string and/or \"password\" JSON string.");
					}
				}else{
					response = PgSqlModel::Error("You cannot gain access to this.");
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
