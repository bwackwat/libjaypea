#include <signal.h>

#include "http-api.hpp"
#include "pgsql-model.hpp"
#include "tls-websocket-server.hpp"


int main(int argc, char **argv){
	std::string hostname;
	std::string admin;
	std::string public_directory;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int http_port;
	int https_port;
	int chat_port;
	int cache_megabytes = 30;
	std::string connection_string;
	
	
	///////////////////////////////////////////////////////
	//	CONFIGURE ARGUMENTS
	///////////////////////////////////////////////////////
	

	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("admin", hostname, {"-a"});
	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("http_port", &http_port, {"-p"});
	Util::define_argument("https_port", &https_port, {"-sp"});
	Util::define_argument("chat_port", &chat_port, {"-cp"});
	Util::define_argument("cache_megabytes", &cache_megabytes,{"-cm"});
	Util::define_argument("postgresql_connection", connection_string, {"-pcs"});
	Util::parse_arguments(argc, argv, "This is an HTTP(S) JSON API which hold routes for jph2.net.");
	
	
	///////////////////////////////////////////////////////
	//	INITIALIZE HTTPS SERVER AND API
	///////////////////////////////////////////////////////
	
	
	TlsEpollServer server(ssl_certificate, ssl_private_key, static_cast<uint16_t>(https_port), 10);
	SymmetricEncryptor encryptor;
	HttpApi api(public_directory, &server, &encryptor);
	api.set_file_cache_size(cache_megabytes);
	
	
	///////////////////////////////////////////////////////
	//	SETUP DATABASE MODELS
	///////////////////////////////////////////////////////

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
	
	/*
	PgSqlModel* poi = new PgSqlModel(connection_string, "poi", {
		new Column("id", COL_AUTO),
		new Column("owner_id"),
		new Column("label"),
		new Column("description"),
		new Column("location"),
		new Column("deleted", COL_AUTO | COL_HIDDEN),
		new Column("modified", COL_AUTO),
		new Column("created", COL_AUTO)
	}, ACCESS_USER);
	*/
	
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
	
	///////////////////////////////////////////////////////
	//	START API ROUTES
	///////////////////////////////////////////////////////
	
	
	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API V1!\",\n\"routes\":" + api.routes_string + "}";
	});
	
	
	api.route("POST", "/login", [&](JsonObject* json)->std::string{
		JsonObject* token = users->Access("username", json->GetStr("username"), Util::hash_value_argon2d(json->GetStr("password")));
		std::string response;
		if(token->objectValues.count("error")){
			response = token->stringify();
		}else{
			response = "{\"token\":" + JsonObject::escape(encryptor.encrypt(token->stringify())) + "}";
		}
		return response;
	}, {{"username", STRING}, {"password", STRING}}, std::chrono::seconds(1));
	
	
	api.route("POST", "/token", [&](JsonObject*, JsonObject*)->std::string{
		return "{\"result\":\"Token is good\"}";
	});
	
	
	api.route("POST", "/get/users", [&](JsonObject*, JsonObject* token)->std::string{
		JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1"){
				return users->All()->stringify();
			}
		}
		
		return INSUFFICIENT_ACCESS;
	});
	
	
	api.route("POST", "/get/my/user", [&](JsonObject*, JsonObject* token)->std::string{
		return users->Where("id",  token->GetStr("id"))->stringify();
	});
	
	
	api.route("POST", "/user", [&](JsonObject* json)->std::string{
		JsonObject* temp_users = users->All();
		
		if(json->objectValues["values"]->HasObj("password", STRING)){
			json->objectValues["values"]->objectValues["password"]->stringValue = 
				Util::hash_value_argon2d(json->objectValues["values"]->objectValues["password"]->stringValue);
		}
		
		JsonObject* newuser = users->Insert(json->objectValues["values"]->objectValues);
		
		if(newuser->objectValues.count("error") == 0){
			if(temp_users->arrayValues.size() == 0){
				access->Insert(std::unordered_map<std::string, JsonObject*> {
					{"owner_id", new JsonObject(newuser->GetStr("id"))},
					{"access_types_id", new JsonObject("1")}});
			}
		}
		
		return newuser->stringify();
	}, {{"values", OBJECT}}, std::chrono::hours(1)); // Can only register every 1 hours.
	
	
	api.route("PUT", "/user", [&](JsonObject* json, JsonObject* token)->std::string{
		if(json->objectValues["values"]->objectValues.count("id")){
			json->objectValues.erase("id");
		}
		if(json->objectValues["values"]->objectValues.count("password")){
			json->objectValues["values"]->objectValues["password"]->stringValue = 
				Util::hash_value_argon2d(json->objectValues["values"]->objectValues["password"]->stringValue);
		}
		
		users->Update(token->GetStr("id"), json->objectValues["values"]->objectValues);
		
		JsonObject* new_user = new JsonObject(OBJECT);
		new_user->objectValues["token"] = 
			new JsonObject(JsonObject::escape(encryptor.encrypt(
				users->Where("id", token->GetStr("id"))->arrayValues[0]->stringify())));
		
		return new_user->stringify();
	}, {{"values", OBJECT}}, std::chrono::seconds(2));
	
	
	///////////////////////////////////////////////////////
	//	START OTHER ROUTES
	///////////////////////////////////////////////////////
	
	
	api.route("POST", "/my/access", [&](JsonObject*, JsonObject* token)->std::string{
		return access->Execute("SELECT access_types.id, access_types.name, access_types.description FROM access_types, access WHERE access.access_type_id = access_types.id AND access.owner_id = " + token->GetStr("id") + ";")->stringify();
	});
	
	
	///////////////////////////////////////////////////////
	//	START THREAD ROUTES
	///////////////////////////////////////////////////////
	
	
	api.route("POST", "/my/threads", [&](JsonObject*, JsonObject* token)->std::string{
		return threads->Where("owner_id", token->GetStr("id"))->stringify();
	});
	
	
	api.route("GET", "/thread", [&](JsonObject* json)->std::string{
		return threads->Where("id", json->GetStr("id"))->stringify();
	}, {{"id", STRING}});
	
	
	api.route("POST", "/thread", [&](JsonObject* json, JsonObject* token)->std::string{
		bool has_access = false;
		JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1" ||
			(*it)->GetStr("access_type_id") == "2"){
				has_access = true;
			}
		}
		if(!has_access){
			return "{\"error\":\"This requires blogger access.\"}";
		}
		
		json->objectValues["values"]->objectValues["owner_id"] = token->objectValues["id"];
		
		return threads->Insert(json->objectValues["values"]->objectValues)->stringify();
	}, {{"values", OBJECT}}, std::chrono::seconds(2));
	
	
	api.route("PUT", "/thread", [&](JsonObject* json, JsonObject* token)->std::string{	
		JsonObject* new_thread = threads->Where("id", json->GetStr("id"));
		
		if(new_thread->arrayValues.size() == 0){
			return NO_SUCH_ITEM;
		}
		
		if(new_thread->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
			return INSUFFICIENT_ACCESS;
		}
		
		return threads->Update(json->GetStr("id"), json->objectValues["values"]->objectValues)->stringify();
	}, {{"id", STRING}, {"values", OBJECT}}, std::chrono::seconds(2));
	
	
	api.route("DELETE", "/thread", [&](JsonObject* json, JsonObject* token)->std::string{
		JsonObject* new_thread = threads->Where("id", json->GetStr("id"));
		
		if(new_thread->arrayValues.size() == 0){
			return NO_SUCH_ITEM;
		}
		
		if(new_thread->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
			return INSUFFICIENT_ACCESS;
		}
		
		return threads->Delete(json->GetStr("id"))->stringify(false);
	}, {{"id", STRING}});
	
	
	///////////////////////////////////////////////////////
	//	START MESSAGE ROUTES
	///////////////////////////////////////////////////////
	
	
	api.route("GET", "/thread/messages", [&](JsonObject* json)->std::string{
		return messages->Where("thread_id", json->GetStr("id"))->stringify();
	}, {{"id", STRING}});
	
	
	api.route("GET", "/message", [&](JsonObject* json)->std::string{
		return messages->Where("id", json->GetStr("id"))->stringify();
	}, {{"id", STRING}});
	
	
	api.route("GET", "/thread/messages/by/name", [&](JsonObject* json)->std::string{
		JsonObject* temp_threads = threads->Where("name", json->GetStr("name"));
		
		if(temp_threads->arrayValues.size() == 0){
			return NO_SUCH_ITEM;
		}
		
		return messages->Where("thread_id", temp_threads->arrayValues[0]->GetStr("id"))->stringify();
	}, {{"name", STRING}});
	
	
	api.route("POST", "/message", [&](JsonObject* json, JsonObject* token)->std::string{
		bool has_access = false;
		JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1" ||
			(*it)->GetStr("access_type_id") == "2"){
				has_access = true;
			}
		}
		
		if(!has_access){
			return "{\"error\":\"This requires blogger access.\"}";
		}
		
		json->objectValues["values"]->objectValues["owner_id"] = token->objectValues["id"];
			
		return messages->Insert(json->objectValues["values"]->objectValues)->stringify();
	}, {{"values", OBJECT}}, std::chrono::seconds(2));
	
	
	api.route("PUT", "/message", [&](JsonObject* json, JsonObject* token)->std::string{
		JsonObject* new_message = messages->Where("id", json->GetStr("id"));
		
		if(new_message->arrayValues.size() == 0){
			return NO_SUCH_ITEM;
		}
		
		if(new_message->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
			return INSUFFICIENT_ACCESS;
		}
		
		return messages->Update(json->GetStr("id"), json->objectValues["values"]->objectValues)->stringify();
	}, {{"id", STRING}, {"values", OBJECT}}, std::chrono::seconds(2));
	
	
	api.route("DELETE", "/message", [&](JsonObject* json, JsonObject* token)->std::string{
		JsonObject* new_message = messages->Where("id", json->GetStr("id"));
		
		if(new_message->arrayValues.size() == 0){
			return NO_SUCH_ITEM;
		}
		
		if(new_message->arrayValues[0]->GetStr("owner_id") != token->GetStr("id")){
			return INSUFFICIENT_ACCESS;
		}
		
		return messages->Delete(json->GetStr("id"))->stringify();
	}, {{"id", STRING}});
	
	
	///////////////////////////////////////////////////////
	//	START CHAT ROUTES
	///////////////////////////////////////////////////////
	
	
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
	
	
	api.route("GET", "/chat", [&](JsonObject*)->std::string{
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
	});
	
	
	api.route("POST", "/user/chat", [&](JsonObject* json, JsonObject* token)->std::string{
	
		JsonObject* msg = new JsonObject(OBJECT);
		msg->objectValues["color"] = new JsonObject(token->GetStr("color"));
		msg->objectValues["handle"] = new JsonObject(token->GetStr("username"));
		msg->objectValues["message"] = new JsonObject(json->GetStr("message"));
		
		return chat_msg(msg);
	}, {{"message", STRING}}, std::chrono::seconds(2));
	
	
	api.route("POST", "/chat", [&](JsonObject* json)->std::string{
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
	}, {{"handle", STRING}, {"message", STRING}}, std::chrono::seconds(2));
	
	
	///////////////////////////////////////////////////////
	//	SETUP AND START HTTP REDIRECTER
	///////////////////////////////////////////////////////
	
	
	EpollServer http_server(static_cast<uint16_t>(http_port), 10);
	http_server.on_read = [&](int fd, const char* data, ssize_t)->ssize_t{
		const char* it = data;
		int state = 1;
		std::string url = "";
		url += hostname;

		for(; *it; ++it){
			if(state == 2){
				if(*it == '\r' || *it == '\n' || *it == ' '){
					break;
				}else{
					url += *it;
				}
			}else if(*it == ' '){
				state = 2;
			}
		}
		
		std::string response = Util::get_redirection_html(url);
		const char* http_response = response.c_str();
		size_t http_response_length = response.length();
		
		http_server.send(fd, http_response, http_response_length);
		return -1;
	};
	http_server.run(true, 1);
	
	
	///////////////////////////////////////////////////////
	//	SETUP AND START GAME TICK SERVER
	///////////////////////////////////////////////////////
	
	
	TlsWebsocketServer game_server(ssl_certificate, ssl_private_key, static_cast<uint16_t>(chat_port), 10);	
	std::unordered_map<int, JsonObject*> client_details;
	JsonObject game_state(OBJECT);
	game_state.objectValues["players"] = new JsonObject(OBJECT);
	
	// Pixels per tick.
	static const double maxSpeed = 15.0;
	static const double acceleration = 0.04;
	static const double maxTurnSpeed = 0.1;
	static const double turnAcceleration = 0.001;
	
	// A type of "event" queue for disconnects.
	std::queue<int> disconnect_queue;
	
	std::thread tick_thread([&]{
		// 1000ms per second, 60 tick rate, about 16ms.
		std::chrono::milliseconds ms_per_tick = std::chrono::milliseconds(16);
		//ms_per_tick += std::chrono::milliseconds(0);
		
		std::chrono::milliseconds tick_ms = std::chrono::milliseconds(0);
		
		// Calculate a game tick every ms_per_tick.
		// Can also use std::chrono::high_resolution_clock::now()
		// instead of std::chrono::system_clock::now()
		while(true){
			// Sleep the remainder ms.
			std::this_thread::sleep_for(tick_ms);
			
			// Start time of tick.
			tick_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch());
			
			//Run the simulation.
			for(auto iter = game_state["players"]->objectValues.begin(); iter != game_state["players"]->objectValues.end(); ++iter){
				//DEBUG("INP PLAYER " << iter->second->GetStr("input"))
				char direction = iter->second->GetStr("i")[0];
				char turn = iter->second->GetStr("i")[1];
				
				double x = std::stod(iter->second->GetStr("x"));
				double y = std::stod(iter->second->GetStr("y"));
				double speed = std::stod(iter->second->GetStr("s"));
				
				if(direction == 'f'){
					if(speed < maxSpeed){
						speed += acceleration;
					}
				}else if(direction == 'b'){
					if(speed > -maxSpeed){
						speed -= acceleration;
					}
				}else{
					if(std::abs(speed) <= acceleration){
						speed = 0;
					}else if(speed > 0){
						speed -= acceleration;
					}else if(speed < 0){
						speed += acceleration;
					}
				}
				
				// Handle turning.
				
				double rotation = std::stod(iter->second->GetStr("r"));
				double turnSpeed = std::stod(iter->second->GetStr("ts"));
				
				//DEBUG(turn)
				if(turn == 'l'){
					if(turnSpeed < maxTurnSpeed){
						turnSpeed += turnAcceleration;
					}
					rotation += turnSpeed;
				}else if(turn == 'r'){
					if(turnSpeed < maxTurnSpeed){
						turnSpeed += turnAcceleration;
					}
					rotation -= turnSpeed;
				}else{
					if(turnSpeed <= turnAcceleration){
						turnSpeed = 0;
					}else if(turnSpeed > 0){
						turnSpeed -= turnAcceleration;
					}
				}
				
				x += speed * std::cos(rotation);
				y += speed * -1 * std::sin(rotation);
				
				iter->second->objectValues["x"]->stringValue = std::to_string(x);
				iter->second->objectValues["y"]->stringValue = std::to_string(y);
				iter->second->objectValues["s"]->stringValue = std::to_string(speed);
				iter->second->objectValues["r"]->stringValue = std::to_string(rotation);
				iter->second->objectValues["ts"]->stringValue = std::to_string(turnSpeed);
			}
			
			// Broadcast the game state.
			std::string bcast_msg = game_state.stringify();
			//DEBUG("bcast_msg" << bcast_msg)
			
			while(!disconnect_queue.empty()){
				int fd = disconnect_queue.front();
				disconnect_queue.pop();
				delete game_state["players"]->objectValues[client_details[fd]->GetStr("handle")];
				game_state["players"]->objectValues.erase(client_details[fd]->GetStr("handle"));
				client_details.erase(fd);
			}

			for(auto iter = client_details.begin(); iter != client_details.end(); ++iter){
				//DEBUG("BC: " << iter->first)
				game_server.send(iter->first, bcast_msg.c_str(), static_cast<size_t>(bcast_msg.length()));
			}
			
			// Time per tick minus the duration of the tick calculations, gives time to wait before next tick.
			tick_ms = ms_per_tick - (std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()) - tick_ms);
			
			if(tick_ms < std::chrono::milliseconds(0)){
				DEBUG("CALCULATIONS TOOK LONGER THAN A TICK!!!")
				tick_ms = std::chrono::milliseconds(0);
			}
		}
	});
	tick_thread.detach();
	
	// END GAME STATE MANAGEMENT
	
	game_server.on_disconnect = [&](int fd){
		DEBUG("DISC: " << fd)
		if(client_details.count(fd)){
			disconnect_queue.push(fd);
			DEBUG("DO DISC: " << client_details[fd]->GetStr("handle"))
		}
	};
	
	// TODO: Only receive inputs from client. Writing data here at the same time as above causes corruption.
	
	game_server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		#if defined(DO_DEBUG)
			// Simulates latency.
			//std::this_thread::sleep_for(std::chrono::milliseconds(35));
			//PRINT("RECV:" << data << "|ENDRECV")
			std::stringstream dbg;
			int total = 0;
			for(size_t i = 0; i < static_cast<size_t>(data_length); i++){
				if(static_cast<int>(data[i]) < 32 || static_cast<int>(data[i]) > 126){
					dbg << static_cast<int>(data[i]) << "@" << i << ", ";
					total++;
				}
			}
			if(total > 0){
				Util::print_bits(data, static_cast<size_t>(data_length));
				PRINT("dbg.str()" << dbg.str())
				PRINT("total" << total)
			}
		#endif
		
		JsonObject* msg = new JsonObject();
		msg->parse(data);
		
		if(msg->HasObj("handle", STRING) &&
		msg->HasObj("i", STRING)){
			if(game_state["players"]->objectValues.count(msg->GetStr("handle"))){
				game_state["players"]->objectValues[msg->GetStr("handle")]
					->objectValues["i"]->stringValue = msg->GetStr("i");
			}
			//DEBUG("INPU: " << msg->GetStr("input"));
		}else if(msg->HasObj("handle", STRING) &&
		msg->HasObj("x", STRING) &&
		msg->HasObj("y", STRING)){
			if(!client_details.count(fd)){
				JsonObject* player = new JsonObject(OBJECT);
				player->objectValues["x"] = new JsonObject(msg->GetStr("x"));
				player->objectValues["y"] = new JsonObject(msg->GetStr("y"));
				player->objectValues["s"] = new JsonObject("0");
				player->objectValues["r"] = new JsonObject("0");
				player->objectValues["ts"] = new JsonObject("0");
				player->objectValues["i"] = new JsonObject("nn");
				//player->objectValues["c"] = new JsonObject(msg->GetStr("color"));
				DEBUG(player->stringify())
				game_state["players"]->objectValues[msg->GetStr("handle")] = player;
				
				client_details[fd] = new JsonObject(OBJECT);
				client_details[fd]->objectValues["handle"] = new JsonObject(msg->GetStr("handle"));
			}
			
		}else{
			DEBUG("JUNK: " << msg->stringify())
		}
		return data_length;
	};
	
	game_server.run(true, 1);
	
	#if defined(DO_DEBUG)
		api.route("GET", "/end", [&](JsonObject*)->std::string{
			game_server.running = false;
			server.running = false;
			http_server.running = false;
			return "APPLICATION ENDING";
		});
	#endif
	
	api.start();
	
	delete users;
	//delete poi;
	delete threads;
	delete messages;
	//delete access_types;
	delete access;
	
	Util::clean();
	
	PRINT("DONE!")
}

