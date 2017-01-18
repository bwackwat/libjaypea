#pragma once

#include <vector>
#include <string>

#include <pqxx/pqxx>

#include "json.hpp"

// http://stackoverflow.com/questions/18591924/how-to-use-bitmask
#define COL_REG 0
#define COL_AUTO 1

class Column{
public:
	const char* name;
	unsigned char flags;

	Column(const char* new_name, unsigned char new_flags = COL_REG)
	:name(new_name), flags(new_flags){}
};

class PgSqlModel {
public:
	const std::string table;

	PgSqlModel(std::string new_conn, std::string new_table, std::vector<Column*> new_colss);

	static JsonObject* Error(std::string message);
	bool HasColumn(std::string name);

	JsonObject* All();
	JsonObject* Where(std::string key, std::string value);
	JsonObject* Insert(std::vector<JsonObject*> values);
	JsonObject* Update(std::string id, std::unordered_map<std::string, JsonObject*> values);
private:
	pqxx::connection conn;
	std::vector<Column*> cols;
};
