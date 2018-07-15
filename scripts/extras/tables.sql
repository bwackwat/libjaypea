
DROP TABLE users;
CREATE TABLE users (
	id SERIAL PRIMARY KEY,
	username VARCHAR(50) UNIQUE NOT NULL,
	password TEXT NOT NULL,
	primary_token TEXT,
	secondary_token TEXT,
	email VARCHAR(100) UNIQUE NOT NULL,
	first_name VARCHAR(50) NOT NULL,
	last_name VARCHAR(50) NOT NULL,
	verified BOOLEAN NOT NULL,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE users ALTER verified SET DEFAULT FALSE;
ALTER TABLE users ALTER modified SET DEFAULT now();
ALTER TABLE users ALTER created SET DEFAULT now();

CREATE EXTENSION postgis;

DROP TABLE poi;
CREATE TABLE poi (
	id SERIAL PRIMARY KEY,
	owner_id SERIAL NOT NULL,
	label TEXT NOT NULL,
	description TEXT NOT NULL,
	location GEOGRAPHY(POINT, 4326) NOT NULL,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE poi ALTER modified SET DEFAULT now();
ALTER TABLE poi ALTER created SET DEFAULT now();

DROP TABLE threads;
CREATE TABLE threads (
	id SERIAL PRIMARY KEY,
	owner_id SERIAL NOT NULL,
	name VARCHAR(50) NOT NULL,
	description TEXT NOT NULL,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE threads ALTER modified SET DEFAULT now();
ALTER TABLE threads ALTER created SET DEFAULT now();

DROP TABLE messages;
CREATE TABLE messages (
	id SERIAL PRIMARY KEY,
	owner_id SERIAL NOT NULL,
	title TEXT NOT NULL,
	content TEXT NOT NULL,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE messages ALTER modified SET DEFAULT now();
ALTER TABLE messages ALTER created SET DEFAULT now();

DROP TABLE access_types;
CREATE TABLE access_types (
	id SERIAL PRIMARY KEY,
	name VARCHAR(50) NOT NULL,
	description TEXT NOT NULL,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE access_types ALTER modified SET DEFAULT now();
ALTER TABLE access_types ALTER created SET DEFAULT now();

DROP TABLE access;
CREATE TABLE access (
	owner_id SERIAL NOT NULL,
	access_type_id SERIAL NOT NULL,
	UNIQUE(owner_id, access_type_id)
);
ALTER TABLE access ALTER modified SET DEFAULT now();
ALTER TABLE access ALTER created SET DEFAULT now();

CREATE ROLE bwackwat WITH LOGIN;
GRANT SELECT, INSERT, UPDATE ON users TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON poi TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON threads TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON messages TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON access_types TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON access TO bwackwat;

GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO bwackwat;
ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT USAGE, SELECT ON SEQUENCES TO bwackwat;

