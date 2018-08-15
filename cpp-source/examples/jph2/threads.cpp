
auto create_thread = [&](JsonObject* json, JsonObject* token)->std::string{
	bool has_access = false;
	JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
	for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
		if((*it)->GetStr("access_type_id") == "1" ||
		(*it)->GetStr("access_type_id") == "2"){
			has_access = true;
			break;
		}
	}
	
	if(!has_access){
		return "{\"error\":\"This requires blogger access.\"}";
	}
	
	json->objectValues["values"]->objectValues["owner_id"] = token->objectValues["id"];
	
	return threads->Insert(json->objectValues["values"]->objectValues)->stringify();
};

auto read_thread = [&](JsonObject* json)->std::string{
	return threads->Where("id", json->GetStr("id"))->stringify();
};

auto update_thread = [&](JsonObject* json, JsonObject* token)->std::string{	
	JsonObject* new_thread = threads->Where("id", json->GetStr("id"));
	
	if(new_thread->arrayValues.size() == 0){
		return NO_SUCH_ITEM;
	}
	
	if(new_thread->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
		return INSUFFICIENT_ACCESS;
	}
	
	return threads->Update(json->GetStr("id"), json->objectValues["values"]->objectValues)->stringify();
};

auto delete_thread = [&](JsonObject* json, JsonObject* token)->std::string{
	JsonObject* new_thread = threads->Where("id", json->GetStr("id"));
	
	if(new_thread->arrayValues.size() == 0){
		return NO_SUCH_ITEM;
	}
	
	if(new_thread->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
		return INSUFFICIENT_ACCESS;
	}
	
	return threads->Delete(json->GetStr("id"))->stringify();
};

