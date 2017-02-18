#include <vector>
#include <string>

#include "argon2.h"

#include "util.hpp"
#include "pgsql-model.hpp"
#include "symmetric-epoll-server.hpp"

static std::string hash_value_argon2d(std::string password){
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
	std::string admin = "admin";

	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("postgresql_connection", connection_string, {"-pcs"});
	Util::define_argument("admin", admin, {"-a"});
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

	std::unordered_map<std::string, PgSqlModel*> db_tables = {
		{users->table, users},
		{poi->table, poi},
		{posts->table, posts}
	};
	
	SymmetricEpollServer server(keyfile, 20000, 1);
	
	SymmetricEncryptor encryptor;

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT(data)
		JsonObject* request = new JsonObject();
		request->parse(data);
		JsonObject* response = 0;
		PRINT(request->stringify(true))

		if(request->objectValues.count("table") &&
		request->objectValues.count("token") &&
		db_tables.count(request->objectValues["table"]->stringValue),
		request->objectValues.count("operation")){
			std::string& table_name = request->objectValues["table"]->stringValue;
			PgSqlModel* table = db_tables[table_name];
			std::string& operation = request->objectValues["operation"]->stringValue;
			if(operation == "all"){
				if(table->access_flags > 0){
					JsonObject token;
					try{
						token.parse(encryptor.decrypt(request->objectValues["token"]->stringValue).c_str());
					}catch(const std::exception& e){
						PRINT(e.what())
						response = PgSqlModel::Error("You cannot gain access to this.");
					}
					if(token.objectValues["username"]->stringValue != admin){
						response = PgSqlModel::Error("You cannot gain access to this.");
					}
				}
				if(response == 0){
					response = table->All();
				}
			}else if(operation == "where"){
				if(request->HasObj("key", STRING) &&
				request->HasObj("value", STRING)){
					if(table->access_flags & ACCESS_ADMIN){
						JsonObject token;
						try{
							token.parse(encryptor.decrypt(request->objectValues["token"]->stringValue).c_str());
						}catch(const std::exception& e){
							PRINT(e.what())
							response = PgSqlModel::Error("You cannot gain access to this.");
						}
						if(token.objectValues["username"]->stringValue != admin){
							response = PgSqlModel::Error("You cannot gain access to this.");
						}
					}else if(table->access_flags & ACCESS_USER &&
					request->objectValues["key"]->stringValue == "username"){
						JsonObject* user = users->Where("username", request->GetStr("value"));
						response = table->Where("owner_id", user->arrayValues[0]->GetStr("id"));
						delete user;
					}
					if(response == 0){
						response = table->Where(request->GetStr("key"), request->GetStr("value"));
					}
				}else{
					response = PgSqlModel::Error("Missing \"key\" and/or \"value\" JSON strings.");
				}
			}else if(operation == "insert"){
				if(request->HasObj("values", ARRAY)){
					if(request->objectValues["values"]->arrayValues.size() != table->num_insert_values){
						response = PgSqlModel::Error("Incorrect number of values");
					}else if(table->access_flags & ACCESS_USER){
						JsonObject token;
						try{
							token.parse(encryptor.decrypt(request->GetStr("token")).c_str());
							PRINT(token.stringify(true))
							// Since this table contains owner_id, get the correct id from the valid token.
							request->objectValues["values"]->arrayValues.insert(
								request->objectValues["values"]->arrayValues.begin(),
								new JsonObject(token.GetStr("id")));
						}catch(const std::exception& e){
							PRINT(e.what())
							response = PgSqlModel::Error("You cannot gain access to this.");
						}
					}
					if(response == 0){
						// TODO: Password protected tables hash values for column 1!
						if(table->HasColumn("password")){
							request->objectValues["values"]->arrayValues[1]->stringValue =
								hash_value_argon2d(request->objectValues["values"]->arrayValues[1]->stringValue);
						}
						PRINT(request->stringify(true))
						response = table->Insert(request->objectValues["values"]->arrayValues);
					}
				}else{
					response = PgSqlModel::Error("Missing \"values\" JSON array.");
				}
			}else if(operation == "update"){
				if(request->HasObj("id", STRING) &&
				request->HasObj("values", OBJECT)){
					if(table->access_flags > 0){
						JsonObject token;
						try{
							token.parse(encryptor.decrypt(request->GetStr("token")).c_str());
							 PRINT("TOKEN: " + encryptor.decrypt(request->GetStr("token")));
							if(table->access_flags & ACCESS_USER){
								// You need to have a token with the same owner_id.
								if(!table->IsOwner(request->GetStr("id"), token.GetStr("id")) &&
								token.objectValues["username"]->stringValue != admin){
									response = PgSqlModel::Error("You cannot gain access to this.");
								}
							}else{
								// You need to have a token with the same id.
								if(token.GetStr("id") != request->GetStr("id") &&
								token.objectValues["username"]->stringValue != admin){
									response = PgSqlModel::Error("You cannot gain access to this.");
								}	
							}
						}catch(const std::exception& e){
							PRINT(e.what())
							response = PgSqlModel::Error("You cannot gain access to this.");
						}
					}
					if(response == 0){
						response = table->Update(request->objectValues["id"]->stringValue, request->objectValues["values"]->objectValues);
					}
				}else{
					response = PgSqlModel::Error("Missing \"id\" JSON string and/or \"values\" JSON object.");
				}
			}else if(operation == "login"){
				if(table_name == "users"){
					if(request->HasObj("username", STRING) &&
					request->HasObj("password", STRING)){
		      			JsonObject* token = table->Access("username", request->GetStr("username"), hash_value_argon2d(request->GetStr("password")));
						if(token->objectValues.count("error")){
							response = token;
						}else{
							// Is it unnecessary to verify the password on each token use? I guess successful decryption of the token is good enough.
			      			// std::string password = hash_value_argond2d(request->GetStr("password"));
							// token->objectValues["verify"] = new JsonObject(hash_value_sha256(password.substr(password.length() / 2)));
						
							response = new JsonObject(OBJECT);
							response->objectValues["id"] = new JsonObject(token->GetStr("id"));
							response->objectValues["token"] = new JsonObject(encryptor.encrypt(token->stringify()));
							delete token;
						}
					}else{
						response = PgSqlModel::Error("Missing \"username\" and/or \"password\" JSON strings.");
					}
				}else{
					response = PgSqlModel::Error("You cannot gain access to this.");
				}
			}else{
				response = PgSqlModel::Error("Invalid \"operation\" " + operation + ".");
			}
		}else{
			response = PgSqlModel::Error("Missing \"table\", \"token\", and/or \"operation\" JSON strings.");
		}

		if(!response->objectValues.count("error") &&
		!response->objectValues.count("result")){
			response->objectValues["result"] = new JsonObject("Success!");
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
