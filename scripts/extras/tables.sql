
CREATE EXTENSION postgis;

DROP TABLE poi;
DROP TABLE messages;
DROP TABLE threads;
DROP TABLE access;
DROP TABLE access_types;
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
	color VARCHAR(20) NOT NULL,
	verified BOOLEAN NOT NULL,
	deleted TIMESTAMP,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE users ALTER verified SET DEFAULT FALSE;
ALTER TABLE users ALTER modified SET DEFAULT now();
ALTER TABLE users ALTER created SET DEFAULT now();
ALTER TABLE users ALTER color SET DEFAULT 'gold';

CREATE TABLE poi (
	id SERIAL PRIMARY KEY,
	owner_id SERIAL NOT NULL REFERENCES users (id),
	label TEXT NOT NULL,
	description TEXT NOT NULL,
	location GEOGRAPHY(POINT, 4326) NOT NULL,
	deleted TIMESTAMP,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE poi ALTER modified SET DEFAULT now();
ALTER TABLE poi ALTER created SET DEFAULT now();

CREATE TABLE threads (
	id SERIAL PRIMARY KEY,
	owner_id SERIAL NOT NULL REFERENCES users (id),
	name VARCHAR(50) NOT NULL,
	description TEXT NOT NULL,
	deleted TIMESTAMP,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE threads ALTER modified SET DEFAULT now();
ALTER TABLE threads ALTER created SET DEFAULT now();

CREATE TABLE messages (
	id SERIAL PRIMARY KEY,
	thread_id SERIAL NOT NULL REFERENCES threads (id),
	owner_id SERIAL NOT NULL REFERENCES users (id),
	title TEXT NOT NULL,
	content TEXT NOT NULL,
	deleted TIMESTAMP,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE messages ALTER modified SET DEFAULT now();
ALTER TABLE messages ALTER created SET DEFAULT now();

CREATE TABLE access_types (
	id SERIAL PRIMARY KEY,
	name VARCHAR(50) NOT NULL,
	description TEXT NOT NULL,
	deleted TIMESTAMP,
	modified TIMESTAMP NOT NULL,
	created TIMESTAMP NOT NULL
);
ALTER TABLE access_types ALTER modified SET DEFAULT now();
ALTER TABLE access_types ALTER created SET DEFAULT now();

INSERT INTO access_types (id, name, description) VALUES (
	1,
	'Administrator',
	'Has complete administrative privilige.'
);

INSERT INTO access_types (id, name, description) VALUES (
	2,
	'Blogger',
	'Has blogging privilege.'
);

CREATE TABLE access (
	owner_id SERIAL NOT NULL REFERENCES users (id),
	access_type_id SERIAL NOT NULL REFERENCES access_types (id),
	UNIQUE(owner_id, access_type_id)
);


CREATE ROLE bwackwat WITH LOGIN;
GRANT SELECT, INSERT, UPDATE ON users TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON poi TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON threads TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON messages TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON access_types TO bwackwat;
GRANT SELECT, INSERT, UPDATE ON access TO bwackwat;

GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO bwackwat;
ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT USAGE, SELECT ON SEQUENCES TO bwackwat;


