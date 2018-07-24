#pragma once

#include <vector>
#include <string>

#include <pqxx/pqxx>

#include "json.hpp"

// http://stackoverflow.com/questions/18591924/how-to-use-bitmask
#define COL_REG 0
#define COL_AUTO 1
#define COL_HIDDEN 2

#define ACCESS_PUBLIC 0
#define ACCESS_ADMIN 1
#define ACCESS_USER 2

class Column{
public:
	const char* name;
	unsigned char flags;

	Column(const char* new_name, unsigned char new_flags = COL_REG)
	:name(new_name), flags(new_flags){}
};

class PgSqlModel {
public:
	size_t num_insert_values;
	const std::string table;
	unsigned char access_flags;

	PgSqlModel(std::string new_conn, std::string new_table, std::vector<Column*> new_cols, unsigned char new_access_flags = ACCESS_PUBLIC);

	JsonObject* AnyResultToJson(pqxx::result* res);
	// Rows (ARRAY of OBJECTS)
	JsonObject* ResultToJson(pqxx::result* res);
	// Single row (OBJECT)
	JsonObject* ResultToJson(pqxx::result::tuple row);
	
	static JsonObject* Error(std::string message);
	bool HasColumn(std::string name);
	
	JsonObject* Execute(std::string sql);
	JsonObject* All();
	JsonObject* Where(std::string key, std::string value);
	JsonObject* Insert(std::vector<JsonObject*> values);
	JsonObject* Delete(std::string id);
	bool IsOwner(std::string id, std::string owner_id);
	JsonObject* Update(std::string id, std::unordered_map<std::string, JsonObject*> values);
	
	// Try to get an access token for the model.
	JsonObject* Access(const std::string& key, const std::string& value, const std::string& password);
private:
	pqxx::connection conn;
	std::vector<Column*> cols;
};
