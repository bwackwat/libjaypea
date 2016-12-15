#include <vector>
#include <string>

#include "pgsql-model.hpp"

int main(){
	PgSqlModel users("connection", "users",
	{"id", "username", "password", "email", "first_name", "last_name"});
}
