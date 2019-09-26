#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <map>
#include <stdexcept>
#include <iostream>

#include "json.hpp"
#include "util.hpp"

//#define JDEBUG(msg) std::cout << msg << std::endl;
#define JDEBUG(msg)
#define PRINT(msg) std::cout << msg << std::endl;

std::map<enum JsonType, std::string> JsonObject::typeString = {
	{ NOTYPE, "No Json" },
	{ STRING, "String" },
	{ OBJECT, "Object" },
	{ ARRAY, "Array" }
};

const char* JsonObject::parse(const char* str){
	std::stringstream ss;
	enum JsonObjectState objState = NOSTATE;
	JsonObject* value;
	const char* it;
	
	JDEBUG("PARSE: " << str)

	for(it = str; *it; ++it){
		JDEBUG("LOOP:" << it)
		switch(*it){
		case '\\':
			++it;
			JDEBUG("ESCAPE TO: " << *it)
			if(*it == 'n' ||
			*it == 'r' ||
			*it == 't'){
				continue;
			}
			break;
		case '"':
			if(this->type == NOTYPE){
				this->type = STRING;
				JDEBUG("START STRING")
				continue;
			}else if(this->type == STRING){
				this->stringValue = ss.str();
				JDEBUG("SET STRING")
				return ++it;
			}else if(this->type == OBJECT){
				if(objState == NOSTATE){
					objState = GETKEY;
					continue;
				}else if(objState == GETKEY){
					objState = GOTKEY;
					JDEBUG("GOT OBJ KEY: " << ss.str())
					continue;
				}
			}else if(this->type == ARRAY){
				JDEBUG(*it << "NEW ARRAY STR")
				value = new JsonObject();
				it = value->parse(it);
				--it;
				arrayValues.push_back(value);
				JDEBUG(*it << "GOT STR: " << value->stringify())
				continue;
			}
			break;
		case '{':
			if(this->type == NOTYPE){
				this->type = OBJECT;
				JDEBUG("START OBJ")
				continue;
			}else if(this->type == ARRAY){
				JDEBUG("PARSE OBJ")
				value = new JsonObject();
				it = value->parse(it);
				arrayValues.push_back(value);
				JDEBUG(*it << "GOT OBJ: " << value->stringify())
			}
			break;
		case '}':
			if(this->type == OBJECT){
				if(objState == NOSTATE){
					JDEBUG(*it << "ENDOBJECT")
					return it;
				}else{
					JDEBUG("EXTRA!!! " << *it)
				}
			}else if(this->type == ARRAY){
				JDEBUG("EXPECTED ] ! or else")
				return it;
			}else{
				JDEBUG("GOT } !?!?!?!?!!?!?")
			}
			break;
		case '[':
			if(this->type == NOTYPE){
				this->type = ARRAY;
				JDEBUG("START ARRAY")
				continue;
			}/*else if(this->type == STRING){
				this->stringValue = ss.str();
				JDEBUG("RESCUE STRING FROM ARRAY START")
				return ++it;
			}*/else if(this->type == ARRAY){
				value = new JsonObject();
				it = value->parse(it);
				arrayValues.push_back(value);
				JDEBUG("GOT ARR: " << value->stringify())
				continue;
			}
			break;
		case ']':
			if(this->type == ARRAY){
				JDEBUG("ARRAYEND")
				return it;
			}/*else if(this->type == STRING){
				this->stringValue = ss.str();
				JDEBUG("RESCUE STRING FROM ARRAY END")
				return it;
			}*/else if(this->type == OBJECT){
				if(objState != GETKEY &&
				objState != GETVALUE){
					JDEBUG("RESCUE OBJECT FROM ARRAY END")
					return it;
				}
			}
			break;
		case ':':
			if(this->type == OBJECT){
				if(objState == GOTKEY){
					JDEBUG("GETTING VAL!")
					value = new JsonObject();
					it = value->parse(it);
					if(value->type != OBJECT && value->type != ARRAY){
						JDEBUG("IS A | " << typeString[value->type])
						--it;
					}
					objectValues[ss.str()] = value;
					JDEBUG("GOT VAL: " << value->stringify() << " FOR " << ss.str())
					ss.str(std::string());
					objState = NOSTATE;
					JDEBUG("... " << it)
				}
			}
			break;
		}
		if(this->type == STRING){
			ss << *it;
			continue;
		}else if(this->type == OBJECT){
			if(objState == GETKEY){
				ss << *it;
				continue;
			}
		}
	}
	return --it;
}

std::string JsonObject::escape(std::string value){
	std::stringstream escaped;
	escaped << '"';
	for(size_t i = 0; i < value.length(); ++i){
		if(static_cast<int>(value[i]) < 32 || static_cast<int>(value[i]) > 126){
			JDEBUG("WEIRD: " << value << " " << i << " " << value[i])
			JDEBUG("WEIRD: " << value << " " << i << " " << std::hex << static_cast<int>(value[i]))
			JDEBUG("WEIRD: " << value << " " << i << " " << std::dec << static_cast<int>(value[i]))
		}
		if(value[i] == '\n'){
			escaped << "\\n";
		}else if(value[i] == '\r'){
			escaped << "\\r";
		}else if(value[i] == '\t'){
			escaped << "\\t";
		}else if(value[i] == '"'){
			escaped << "\\\"";
		}else if(value[i] == '\\'){
			escaped << "\\\\";
		}else{
			escaped << value[i];
		}
	}
	escaped << '"';
	//PRINT(value)
	//PRINT(escaped)
	return escaped.str();
}

std::string JsonObject::deescape(std::string value){
	std::stringstream deescaped;
	for(size_t i = 0; i < value.length(); ++i){
		if(value[i] == '\\' && value[i + 1] == 'n'){
			deescaped << '\n';
			i++;
		}else if(value[i] == '\\' && value[i + 1] == 'r'){
			deescaped << '\r';
			i++;
		}else if(value[i] == '\\' && value[i + 1] == 't'){
			deescaped << '\t';
			i++;
		}else if(value[i] == '\\' && value[i + 1] == '"'){
			deescaped << '"';
			i++;
		}else if(value[i] == '\\' && value[i + 1] == '\\'){
			deescaped << '\\';
			i++;
		}else{
			deescaped << value[i];
		}
	}
	//PRINT(value)
	//PRINT(escaped)
	return deescaped.str();
}

std::string JsonObject::stringify(bool pretty, size_t depth){
	std::stringstream ss;
	std::string result;
	switch(this->type){
	case NOTYPE:
		result = "\"\"";
		break;
	case STRING:
		result = escape(this->stringValue);
		break;
	case OBJECT:
		ss.put('{');
		if(pretty){
			ss << '\n';
			depth++;
		}
		for(auto it = this->objectValues.begin(); it != this->objectValues.end(); ++it){
			if(pretty){
				ss << std::string(depth, '\t');
			}
			ss << escape(it->first) << ':';
			ss << it->second->stringify(pretty, depth);
			if(std::next(it) != this->objectValues.end()){
				ss << ',';
			}
			if(pretty){
				ss << '\n';
			}
		}
		if(pretty){
			depth--;
			ss << std::string(depth, '\t');
		}
		ss << '}';
		result = ss.str();
		break;
	case ARRAY:
		ss << '[';
		if(pretty){
			ss << '\n';
			depth++;
		}
		for(JsonObject* item : this->arrayValues){
			if(pretty){
				ss << std::string(depth, '\t');
			}
			if(item == this->arrayValues.back()){
				ss << item->stringify(pretty, depth);
			}else{
				ss << item->stringify(pretty, depth) << ',';
			}
			if(pretty){
				ss << '\n';
			}
		}
		if(pretty){
			depth--;
			ss << std::string(depth, '\t');
		}
		ss << ']';
		result = ss.str();
		break;
	}
	return result;
}

JsonObject::JsonObject(enum JsonType new_type)
:type(new_type){}

JsonObject::JsonObject(std::string new_stringValue)
:type(STRING), stringValue(new_stringValue){}

// NOTYPE is important in this constructor for the parsing strategy.
JsonObject::JsonObject()
:type(NOTYPE){}

JsonObject::~JsonObject(){
	for(auto it = this->objectValues.begin(); it != this->objectValues.end(); ++it){
		delete it->second;
	}
	for(JsonObject* item : this->arrayValues){
		delete item;
	}
}

bool JsonObject::HasObj(const std::string& key, enum JsonType t){
	return this->objectValues.count(key) && this->objectValues[key]->type == t;
}

std::string JsonObject::GetStr(const char* key){
	if(!this->objectValues.count(key)){
		PRINT("Missing key: " << key)
		throw new std::exception();
	}
	return this->objectValues[key]->stringValue;
}

std::string JsonObject::GetURLEncodedStr(const char* key){
	if(!this->objectValues.count(key)){
		PRINT("Missing key: " << key)
		throw new std::exception();
	}
	return Util::url_encode(this->objectValues[key]->stringValue);
}


std::string JsonObject::GetURLDecodedStr(const char* key){
	if(!this->objectValues.count(key)){
		PRINT("Missing key: " << key)
		throw new std::exception();
	}
	return Util::url_decode(this->objectValues[key]->stringValue);
}
JsonObject* JsonObject::operator[](const char* key){
	return this->objectValues[key];
}

JsonObject* JsonObject::operator[](size_t index){
	return this->arrayValues[index];
}

JsonObject* JsonObject::operator[](const std::string& key){
	return this->objectValues[key];
}
