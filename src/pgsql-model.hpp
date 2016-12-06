#pragma once

#include <vector>
#include <string>

#include <pqxx/pqxx>

#include "json.hpp"

class PgSqlModel {
public:
	JsonObject* Where(std::string key, std::string value);
	void Insert(std::vector<std::string> values);
	PgSqlModel(std::string conn, std::string table, std::vector<std::string> keys);
private:
	pqxx::connection conn;
	std::string table;
	std::vector<std::string> keys;
};
