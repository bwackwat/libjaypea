#pragma once

#include <vector>
#include <string>

#include <pqxx/pqxx>

#include "json.hpp"

class PgSqlModel {
public:
	JsonObject* Where(std::string key, std::string value);
	void Insert(std::vector<std::string> values);
	PgSqlModel(std::string new_conn, std::string new_table, std::vector<std::string> new_keys);
private:
	pqxx::connection conn;
	std::string table;
	std::vector<std::string> keys;
};
