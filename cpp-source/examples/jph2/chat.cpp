
std::deque<JsonObject*> chat_messages;
std::unordered_map<std::string, std::string> client_last_message;
std::mutex message_mutex;

auto chat_msg = [&](JsonObject* msg){
	size_t index = 0;
	while (true) {
		index = msg->GetStr("message").find("iframe", index);
		if(index == std::string::npos)break;
		msg->objectValues["message"]->stringValue = msg->GetStr("message").replace(index, 6, "");
	}
	
	if(msg->GetStr("message").length() < 2){
		return "{\"error\":\"Message too short.\"}";
	}

	if(msg->GetStr("message").length() > 256){
		return "{\"error\":\"Message too long.\"}";
	}

	if(client_last_message.count(msg->GetStr("handle")) &&
	msg->GetStr("message") == client_last_message[msg->GetStr("handle")]){
		return "{\"error\":\"Do not send the same message.\"}";
	}

	client_last_message[msg->GetStr("handle")] = msg->GetStr("message");

	message_mutex.lock();
	chat_messages.push_front(msg);
	
	if(chat_messages.size() > 100){
		chat_messages.pop_back();
	}
	message_mutex.unlock();
	
	return "{\"status\":\"Sent.\"}";
};

auto read_chat = [&](JsonObject*)->std::string{
	JsonObject response(OBJECT);
	response.objectValues["messages"] = new JsonObject(ARRAY);
	message_mutex.lock();
	for(auto it = chat_messages.begin(); it != chat_messages.end(); ++it){
		DEBUG("MESSAGE: " << (*it)->stringify())
		JsonObject* new_msg = new JsonObject(OBJECT);
		if((*it)->HasObj("color", STRING)){
			new_msg->objectValues["color"] = new JsonObject((*it)->GetStr("color"));
		}
		new_msg->objectValues["handle"] = new JsonObject((*it)->GetStr("handle"));
		new_msg->objectValues["message"] = new JsonObject((*it)->GetStr("message"));
		response["messages"]->arrayValues.push_back(new_msg);
	}
	message_mutex.unlock();
	return response.stringify();
};

auto create_authenticated_chat = [&](JsonObject* json, JsonObject* token)->std::string{
	JsonObject* msg = new JsonObject(OBJECT);
	msg->objectValues["color"] = new JsonObject(token->GetStr("color"));
	msg->objectValues["handle"] = new JsonObject(token->GetStr("username"));
	msg->objectValues["message"] = new JsonObject(json->GetStr("message"));
	
	return chat_msg(msg);
};

auto create_chat = [&](JsonObject* json)->std::string{
	if(users->Where("username", json->GetStr("handle"))->arrayValues.size() > 0){
		return "{\"error\":\"That's an existing username. Please login as that user.\"}";
	}else if(json->GetStr("handle").length() < 5){
		return "{\"error\":\"Handle too short.\"}";
	}else if(json->GetStr("handle").length() > 32){
		return "{\"error\":\"Handle too long.\"}";
	}
	
	JsonObject* msg = new JsonObject(OBJECT);
	msg->objectValues["handle"] = new JsonObject(json->GetStr("handle"));
	msg->objectValues["message"] = new JsonObject(json->GetStr("message"));
	
	return chat_msg(msg);
};

