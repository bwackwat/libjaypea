#include <string>
#include <vector>

#include <pqxx/pqxx>

#include "json.hpp"
#include "pgsql-model.hpp"

JsonObject* PgSqlModel::Where(std::string key, std::string value){
	pqxx::work txn(this->conn);
	std::string sql = "SELECT * FROM " + this->table + " WHERE " + key + " = " + txn.quote(value) + ";";
	pqxx::result res = txn.exec(sql);
	txn.commit();

	JsonObject* result_json = new JsonObject(ARRAY);
	for(pqxx::result::size_type i = 0; i < res.size(); ++i){
		JsonObject* next_item = new JsonObject(OBJECT);
		for(size_t j = 0; j < this->keys.size(); ++j){
			next_item->objectValues[this->keys[j]] = new JsonObject(STRING);
			next_item->objectValues[this->keys[j]]->stringValue = res[i][this->keys[j]].c_str();
			result_json->arrayValues.push_back(next_item);
		}
	}
	return result_json;
}

void PgSqlModel::Insert(std::vector<std::string> values){
	pqxx::work txn(this->conn);
	std::stringstream sql;
	sql << "INSERT INTO " << this->table << '(';
	for(size_t i = 0; i < this->keys.size(); ++i){
		if(i < this->keys.size() - 1){
			sql << this->keys[i] << ", ";
		}else{
			sql << this->keys[i] << ") VALUES (DEFAULT, ";
		}
	}
	for(size_t i = 0; i < values.size(); ++i){
		if(i < values.size() - 1){
			sql << txn.quote(values[i]) << ", ";
		}else{
			sql << txn.quote(values[i]) << "RETURNING id;";
		}
	}	
	pqxx::result res = txn.exec(sql.str());
	txn.commit();
}

PgSqlModel::PgSqlModel(std::string conn, std::string table, std::vector<std::string> keys)
:conn(conn),
table(table),
keys(keys){}
