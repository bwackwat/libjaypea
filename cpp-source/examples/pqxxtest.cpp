#include <iostream>
#include <pqxx/pqxx>

int main(){
	pqxx::connection conn("dbname=webservice user=postgres password=abc123");
	pqxx::nontransaction txn(conn);
	pqxx::result res = txn.exec("SELECT access_types.name, access_types.description FROM access_types, access WHERE access.access_type_id = access_types.id AND access.owner_id = 13;");
	txn.commit();
	std::cout << (res[0][0].as<const char*>()) << std::endl;
}
