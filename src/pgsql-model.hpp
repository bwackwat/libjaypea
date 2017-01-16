#pragma once

#include <vector>
#include <string>

#include <pqxx/pqxx>

#include "json.hpp"

class PgSqlModel {
public:
	const std::string table;

	PgSqlModel(std::string new_conn, std::string new_table, std::vector<std::string> new_keys);

	JsonObject* All();
	JsonObject* Where(std::string key, std::string value);
	void Insert(std::vector<JsonObject*> values);
private:
	pqxx::connection conn;
	std::vector<std::string> keys;
};
