
auto login = [&](JsonObject* json)->std::string{
	JsonObject* token = users->Access("username", json->GetStr("username"), Util::hash_value_argon2d(json->GetStr("password")));
	std::string response;
	if(token->objectValues.count("error")){
		response = token->stringify();
	}else{
		response = "{\"token\":" + JsonObject::escape(encryptor.encrypt(token->stringify())) + "}";
	}
	return response;
};

// Any route that is designed with a second parameter (the decrypted token) will prove a good token.
auto check_token = [&](JsonObject*, JsonObject*)->std::string{
	return "{\"result\":\"Token is good\"}";
};

auto create_user = [&](JsonObject* json)->std::string{
	JsonObject* temp_users = users->All();
	
	if(json->objectValues["values"]->HasObj("password", STRING)){
		if(json->objectValues["values"]->objectValues["password"]->stringValue.length() < 16){
			return "{\"error\":\"Password must have at least 16 characters.\"}";
		}
		json->objectValues["values"]->objectValues["password"]->stringValue = 
			Util::hash_value_argon2d(json->objectValues["values"]->objectValues["password"]->stringValue);
	}
	
	JsonObject* new_user = users->Insert(json->objectValues["values"]->objectValues);
	
	if(new_user->objectValues.count("error") == 0){
		if(temp_users->arrayValues.size() == 0){
			access->Insert(std::unordered_map<std::string, JsonObject*> {
				{"owner_id", new JsonObject(new_user->GetStr("id"))},
				{"access_type_id", new JsonObject("1")}});
		}
	}
	
	return new_user->stringify();
};

auto read_users = [&](JsonObject*, JsonObject* token)->std::string{
	JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
	for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
		if((*it)->GetStr("access_type_id") == "1"){
			return users->All()->stringify();
		}
	}
	
	return INSUFFICIENT_ACCESS;
};

auto update_user = [&](JsonObject* json, JsonObject* token)->std::string{
	if(json->objectValues["values"]->objectValues.count("id")){
		json->objectValues.erase("id");
	}
	if(json->objectValues["values"]->objectValues.count("password")){
		if(json->objectValues["values"]->objectValues["password"]->stringValue.length() < 16){
			return "{\"error\":\"Password must have at least 16 characters.\"}";
		}
		json->objectValues["values"]->objectValues["password"]->stringValue = 
			Util::hash_value_argon2d(json->objectValues["values"]->objectValues["password"]->stringValue);
	}
	
	users->Update(token->GetStr("id"), json->objectValues["values"]->objectValues);
	
	JsonObject* new_user = new JsonObject(OBJECT);
	new_user->objectValues["token"] = 
		new JsonObject(JsonObject::escape(encryptor.encrypt(
			users->Where("id", token->GetStr("id"))->arrayValues[0]->stringify())));
	
	return new_user->stringify();
};

auto read_my_user = [&](JsonObject*, JsonObject* token)->std::string{
	return users->Where("id",  token->GetStr("id"))->stringify();
};

auto read_my_access = [&](JsonObject*, JsonObject* token)->std::string{
	return access->Execute("SELECT access_types.id, access_types.name, access_types.description FROM access_types, access WHERE access.access_type_id = access_types.id AND access.owner_id = " + token->GetStr("id") + ";")->stringify();
};

auto read_my_threads = [&](JsonObject*, JsonObject* token)->std::string{
	return threads->Where("owner_id", token->GetStr("id"))->stringify();
};

auto read_my_poi = [&](JsonObject*, JsonObject* token)->std::string{
	return poi->Where("owner_id", token->GetStr("id"))->stringify();
};

