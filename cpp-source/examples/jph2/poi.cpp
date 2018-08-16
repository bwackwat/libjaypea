
auto create_poi = [&](JsonObject* json, JsonObject* token)->std::string{
	json->objectValues["values"]->objectValues["owner_id"] = token->objectValues["id"];
	
	return poi->Insert(json->objectValues["values"]->objectValues)->stringify();
};

auto read_poi = [&](JsonObject* json)->std::string{
	return poi->Where("id", json->GetStr("id"))->stringify();
};

auto update_poi = [&](JsonObject* json, JsonObject* token)->std::string{	
	JsonObject* new_poi = poi->Where("id", json->GetStr("id"));
	
	if(new_poi->arrayValues.size() == 0){
		return NO_SUCH_ITEM;
	}
	
	if(new_poi->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
		return INSUFFICIENT_ACCESS;
	}
	
	return poi->Update(json->GetStr("id"), json->objectValues["values"]->objectValues)->stringify();
};

auto delete_poi = [&](JsonObject* json, JsonObject* token)->std::string{
	JsonObject* new_poi = poi->Where("id", json->GetStr("id"));
	
	if(new_poi->arrayValues.size() == 0){
		return NO_SUCH_ITEM;
	}
	
	if(new_poi->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
		return INSUFFICIENT_ACCESS;
	}
	
	return poi->Delete(json->GetStr("id"))->stringify();
};

auto read_poi_by_username = [&](JsonObject* json)->std::string{
	JsonObject* temp_users = users->Where("username", json->GetStr("username"));
	
	if(temp_users->arrayValues.size() == 0){
		return NO_SUCH_ITEM;
	}
	
	return poi->Where("owner_id", temp_users->arrayValues[0]->GetStr("id"))->stringify();
};

