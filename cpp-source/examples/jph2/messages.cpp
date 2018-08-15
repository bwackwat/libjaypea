
auto create_message = [&](JsonObject* json, JsonObject* token)->std::string{
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
		
	return messages->Insert(json->objectValues["values"]->objectValues)->stringify();
};

auto read_message =  [&](JsonObject* json)->std::string{
	return messages->Where("id", json->GetStr("id"))->stringify();
};

auto update_message = [&](JsonObject* json, JsonObject* token)->std::string{
	JsonObject* new_message = messages->Where("id", json->GetStr("id"));
	
	if(new_message->arrayValues.size() == 0){
		return NO_SUCH_ITEM;
	}
	
	if(new_message->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
		return INSUFFICIENT_ACCESS;
	}
	
	return messages->Update(json->GetStr("id"), json->objectValues["values"]->objectValues)->stringify();
};

auto delete_message = [&](JsonObject* json, JsonObject* token)->std::string{
	JsonObject* new_message = messages->Where("id", json->GetStr("id"));
	
	if(new_message->arrayValues.size() == 0){
		return NO_SUCH_ITEM;
	}
	
	if(new_message->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
		return INSUFFICIENT_ACCESS;
	}
	
	return messages->Delete(json->GetStr("id"))->stringify();
};

auto read_thread_messages = [&](JsonObject* json)->std::string{
	return messages->Where("thread_id", json->GetStr("id"))->stringify();
};

auto read_thread_messages_by_name = [&](JsonObject* json)->std::string{
	JsonObject* temp_threads = threads->Where("name", json->GetStr("name"));
	
	if(temp_threads->arrayValues.size() == 0){
		return NO_SUCH_ITEM;
	}
	
	return messages->Where("thread_id", temp_threads->arrayValues[0]->GetStr("id"))->stringify();
};

