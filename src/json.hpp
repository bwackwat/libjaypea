#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <map>

enum JsonType {
	NOTYPE,
	STRING,
	NUMBER,
	OBJECT,
	ARRAY
};

enum JsonObjectState {
	NOSTATE,
	GETKEY,
	GOTKEY,
	GETVALUE,
	GOTVALUE
};

class JsonObject {
public:
	enum JsonType type;
	static std::map<enum JsonType, std::string> typeString;

	std::string stringValue;
	double numberValue;
	std::map<std::string, JsonObject*> objectValues;
	std::vector<JsonObject*> arrayValues;

	static std::string escape(std::string value);
	const char* parse(const char* str);
	std::string stringify(bool pretty = false, size_t depth = 0);

	JsonObject(enum JsonType new_type);
	JsonObject(std::string new_stringValue);
	JsonObject();
	~JsonObject();

	JsonObject* operator[](const char* index);
};
