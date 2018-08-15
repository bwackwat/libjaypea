
PgSqlModel* users = new PgSqlModel(connection_string, "users", {
	new Column("id", COL_AUTO),
	new Column("username"),
	new Column("password", COL_HIDDEN),
	new Column("primary_token"),
	new Column("secondary_token"),
	new Column("email"),
	new Column("first_name"),
	new Column("last_name"),
	new Column("color"),
	new Column("deleted", COL_AUTO | COL_HIDDEN),
	new Column("modified", COL_AUTO),
	new Column("created", COL_AUTO)
});

PgSqlModel* poi = new PgSqlModel(connection_string, "poi", {
	new Column("id", COL_AUTO),
	new Column("owner_id"),
	new Column("label"),
	new Column("description"),
	new Column("location"),
	new Column("deleted", COL_AUTO | COL_HIDDEN),
	new Column("modified", COL_AUTO),
	new Column("created", COL_AUTO)
});

PgSqlModel* threads = new PgSqlModel(connection_string, "threads", {
	new Column("id", COL_AUTO),
	new Column("owner_id"),
	new Column("name"),
	new Column("description"),
	new Column("deleted", COL_AUTO | COL_HIDDEN),
	new Column("modified", COL_AUTO),
	new Column("created", COL_AUTO)
});

PgSqlModel* messages = new PgSqlModel(connection_string, "messages", {
	new Column("id", COL_AUTO),
	new Column("owner_id"),
	new Column("thread_id"),
	new Column("title"),
	new Column("content"),
	new Column("deleted", COL_AUTO | COL_HIDDEN),
	new Column("modified", COL_AUTO),
	new Column("created", COL_AUTO)
});

/*
PgSqlModel* access_types = new PgSqlModel(connection_string, "access_types", {
	new Column("id", COL_AUTO),
	new Column("name"),
	new Column("description"),
	new Column("deleted", COL_AUTO | COL_HIDDEN),
	new Column("modified", COL_AUTO),
	new Column("created", COL_AUTO)
});
*/

PgSqlModel* access = new PgSqlModel(connection_string, "access", {
	new Column("owner_id"),
	new Column("access_type_id")
});

