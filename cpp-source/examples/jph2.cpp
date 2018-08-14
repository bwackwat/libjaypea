#include <limits>

#include <signal.h>

#include "http-api.hpp"
#include "pgsql-model.hpp"
#include "websocket-server.hpp"
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
					{"access_type_id", new JsonObject("1")}});
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
	
	
	// I don't see a reason why this needs TLS.
	// You can currently spoof other players, but you cant control the movement of players.
	//WebsocketServer game_server(static_cast<uint16_t>(chat_port), 10);
	TlsWebsocketServer game_server(ssl_certificate, ssl_private_key, static_cast<uint16_t>(chat_port), 10);
	
	// Unlike the structure in TcpServer, this structure indicates "joined players".
	// Need to build out a larger UI and corresponding events for players.
	// E.g. players can only connect once they have joined the game?
	std::unordered_map<int, JsonObject*> client_details;
	
	// This data does not need to be broadcasted.
	JsonObject extra_game_state(OBJECT);
	struct Point{
		double x, y;
		Point(double nx, double ny): x(nx), y(ny){}
	};
	// Each vector is a list of points representing the shape vertices named by string.
	std::unordered_map<std::string, std::vector<Point>> collision_edges;
	// These points are properly centered.
	collision_edges["tank"] = {Point(-25, -15), Point(25, -15), Point(25, 15), Point(-15, 15)};
	// Object, not shape specific.
	std::unordered_map<std::string, Point> locations;
	std::unordered_map<std::string, double> rotations;
	std::unordered_map<std::string, double> speeds;
	std::unordered_map<std::string, double> turnSpeeds;
	
	// The public game state broadcasted to all players.
	JsonObject game_state(OBJECT);
	game_state.objectValues["players"] = new JsonObject(OBJECT);
	
	// Pixels per tick.
	static const double maxSpeed = 10.0;
	static const double acceleration = 0.05;
	static const double backAcceleration = 0.03;
	static const double maxTurnSpeed = 0.05;
	static const double turnAcceleration = 0.0005;
	
	struct Event{
		// 0 connect
		// 1 disconnect
		// 3 destroy tank
		byte type;
		
		int fd;
		std::string player;
		
		Event(byte ntype, int nfd, std::string nplayer): type(ntype), fd(nfd), player(nplayer){}
		Event(byte ntype, std::string nplayer): type(ntype), player(nplayer){}
	};
	std::queue<Event *> game_event_queue;
	
	// Server game simulation loop.
	std::thread tick_thread([&]{
		// 1000ms per second, 60 tick rate, about 16ms.
		std::chrono::milliseconds ms_per_tick = std::chrono::milliseconds(16);
		
		// This is to simulate a long calculation of a tick.
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
			
			while(!game_event_queue.empty()){
				Event* event = game_event_queue.front();
				game_event_queue.pop();
				
				if(event->type == 0){
					std::string bcast_msg = "{\"connect\":\"" + event->player + "\"}";
					//if(client_details[event[fd]]->HasObj("color");
					//DEBUG("BCAST: " << bcast_msg)
					for(auto iter = client_details.begin(); iter != client_details.end(); ++iter){
						game_server.send(iter->first, bcast_msg.c_str(), static_cast<size_t>(bcast_msg.length()));
					}
				}else if(event->type == 1){
					std::string bcast_msg = "{\"disconnect\":\"" + event->player + "\"}";
					//DEBUG("BCAST: " << bcast_msg)
					for(auto iter = client_details.begin(); iter != client_details.end(); ++iter){
						game_server.send(iter->first, bcast_msg.c_str(), static_cast<size_t>(bcast_msg.length()));
					}
					
					locations.erase(event->player + "tank");
					rotations.erase(event->player + "tank");
					speeds.erase(event->player + "tank");
					turnSpeeds.erase(event->player + "tank");
					
					// They cannot respawn after this, because their client is bonked.
					delete game_state["players"]->objectValues[event->player + "tank"];
					game_state["players"]->objectValues.erase(event->player + "tank");
				}else if(event->type == 2){
					std::string bcast_msg = "{\"explode\":\"" + event->player + "\"}";
					for(auto iter = client_details.begin(); iter != client_details.end(); ++iter){
						game_server.send(iter->first, bcast_msg.c_str(), static_cast<size_t>(bcast_msg.length()));
					}
					
					locations.erase(event->player);
					rotations.erase(event->player);
					speeds.erase(event->player);
					turnSpeeds.erase(event->player);
					
					// They can respawn after this.
					DEBUG("DELETE " << event->player + "tank")
					delete game_state["players"]->objectValues[event->player];
					game_state["players"]->objectValues.erase(event->player);
				}else{
					DEBUG("UNKNOWN EVENT " << event->type)
				}
				
				delete event;
			}
			
			/// START COLLISION
			
			//DEBUG("START COLLISION")
			for(auto pos_a = locations.begin(); (pos_a != locations.end()); pos_a++){
				// Setup a's edges with tank shape.
				std::vector<Point> a_edges;
				for(auto point = collision_edges["tank"].begin(); (point != collision_edges["tank"].end()); point++){
					a_edges.push_back(Point(pos_a->second.x + point->x, pos_a->second.y + point->y));
				}
				
			
				for(auto pos_b = locations.begin(); (pos_b != locations.end()); pos_b++){
					if(pos_a->first == pos_b->first){
						continue;
					}
				
					//DEBUG("CHECK " << pos_a->first << " WITH " << pos_b->first)
				
					// Setup b's edges with tank shape.
					std::vector<Point> b_edges;
					for(auto point = collision_edges["tank"].begin(); (point != collision_edges["tank"].end()); point++){
						b_edges.push_back(Point(pos_b->second.x + point->x, pos_b->second.y + point->y));
					}
					
					size_t i2;
					bool collision = true;
					double minA, maxA, minB, maxB;
					double projected;
					Point normal(0, 0);
					for(size_t i1 = 0; i1 < a_edges.size(); i1++){
						i2 = (i1 + 1) % a_edges.size();
						
						normal.x = a_edges[i2].y - a_edges[i1].y;
						normal.y = a_edges[i1].x - a_edges[i2].x;
						
						minA = std::numeric_limits<double>::quiet_NaN();
						maxA = std::numeric_limits<double>::quiet_NaN();
						for(auto point : a_edges){
							projected = normal.x * point.x + normal.y * point.y;
							if(std::isnan(minA) || projected < minA){
								minA = projected;
							}
							if(std::isnan(maxA) || projected > maxA){
								maxA = projected;
							}
						}
						
						minB = std::numeric_limits<double>::quiet_NaN();
						maxB = std::numeric_limits<double>::quiet_NaN();
						for(auto point : b_edges){
							projected = normal.x * point.x + normal.y * point.y;
							if(std::isnan(minB) || projected < minB){
								minB = projected;
							}
							if(std::isnan(maxB) || projected > maxB){
								maxB = projected;
							}
						}
						
						//DEBUG(maxA << " < " << minB << " ? || " << maxB << " < " << minA)
						
						if(maxA < minB || maxB < minA){
							collision = false;
							//DEBUG("NO COLLISION")
						}
					}
					
					if(collision){
						game_event_queue.push(new Event(2, pos_a->first));
						//game_event_queue.push(new Event(2, pos_b->first));
						DEBUG(pos_a->first << " COLLIDED WITH " << pos_b->first)
					}
				}
			}
			
			/// END COLLISION
			
			//Calculate tank movement.
			for(auto iter = game_state["players"]->objectValues.begin(); iter != game_state["players"]->objectValues.end(); ++iter){
				//DEBUG("PLAYER INPUT: " << iter->second->GetStr("input"))
				std::string name = iter->first;
				
				// Handle speed.
				char direction = iter->second->GetStr("i")[0];
				
				if(direction == 'f'){
					if(speeds[name] < maxSpeed){
						speeds[name] += acceleration;
					}
				}else if(direction == 'b'){
					if(speeds[name] > -maxSpeed){
						speeds[name] -= acceleration;
					}
				}else{
					if(std::abs(speeds[name]) <= acceleration){
						speeds[name] = 0;
					}else if(speeds[name] > 0){
						speeds[name] -= acceleration;
					}else if(speeds[name] < 0){
						speeds[name] += acceleration;
					}
				}
				
				// Handle turning speed.
				char turn = iter->second->GetStr("i")[1];
				
				if(turn == 'l'){
					if(turnSpeeds[name] < maxTurnSpeed){
						turnSpeeds[name] += turnAcceleration;
					}
					rotations[name] += turnSpeeds[name];
				}else if(turn == 'r'){
					if(turnSpeeds[name] < maxTurnSpeed){
						turnSpeeds[name] += turnAcceleration;
					}
					rotations[name] -= turnSpeeds[name];
				}else{
					if(turnSpeeds[name] <= turnAcceleration){
						turnSpeeds[name] = 0;
					}else if(turnSpeeds[name] > 0){
						turnSpeeds[name] -= turnAcceleration;
					}
				}
				
				// Handle movement
				// TODO: Odd that I cannot use operator[] accessor here.
				locations.at(name).x += speeds[name] * std::cos(rotations[name]);
				locations.at(name).y += speeds[name] * -1 * std::sin(rotations[name]);
				
				iter->second->objectValues["i"]->stringValue = iter->second->GetStr("i");
				iter->second->objectValues["x"]->stringValue = std::to_string(locations.at(name).x);
				iter->second->objectValues["y"]->stringValue = std::to_string(locations.at(name).y);
				iter->second->objectValues["s"]->stringValue = std::to_string(speeds[name]);
				iter->second->objectValues["r"]->stringValue = std::to_string(rotations[name]);
				iter->second->objectValues["ts"]->stringValue = std::to_string(turnSpeeds[name]);
			}
			
			// Broadcast the game state.
			std::string bcast_msg = game_state.stringify();
			//DEBUG("bcast_msg" << bcast_msg)

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
		if(client_details.count(fd)){
			game_event_queue.push(new Event(1, fd, client_details[fd]->GetStr("handle")));
			
			// This is okay because the event will still broadcast.
			client_details.erase(fd);
		}
	};
	
	// This isn't very useful because it doesn't share credentials.
	// game_server.connect = [&](int fd){};
	
	// Writing to the same fd here at the same time as tick loop broadcasts causes corruption.
	game_server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		JsonObject* msg = new JsonObject();
		msg->parse(data);
		
		if(msg->HasObj("handle", STRING) &&
		msg->HasObj("i", STRING)){
			if(game_state["players"]->objectValues.count(msg->GetStr("handle") + "tank")){
				game_state["players"]->objectValues[msg->GetStr("handle") + "tank"]
					->objectValues["i"]->stringValue = msg->GetStr("i");
			}
			//DEBUG("INPU: " << msg->GetStr("input"));
		}else if(msg->HasObj("announce", STRING)){
			if(!client_details.count(fd)){
				client_details[fd] = new JsonObject(OBJECT);
				client_details[fd]->objectValues["handle"] = new JsonObject(msg->GetStr("announce"));
				if(msg->HasObj("color", STRING)){
					client_details[fd]->objectValues["color"] = new JsonObject(msg->GetStr("color"));
				}
				game_event_queue.push(new Event(0, fd, client_details[fd]->GetStr("handle")));
			}
		}else if(msg->HasObj("handle", STRING) &&
		msg->HasObj("x", STRING) &&
		msg->HasObj("y", STRING)){
			if(!game_state["players"]->objectValues.count(msg->GetStr("handle") + "tank")){
			
				// Get their spawn location for game tick.
				locations.emplace(msg->GetStr("handle") + "tank",
					Point(std::stod(msg->GetStr("x")), std::stod(msg->GetStr("y"))));
				// Setup other game tick details.
				rotations.emplace(msg->GetStr("handle") + "tank", 0.0);
				speeds.emplace(msg->GetStr("handle") + "tank", 0.0);
				turnSpeeds.emplace(msg->GetStr("handle") + "tank", 0.0);
				
				JsonObject* player = new JsonObject(OBJECT);
				player->objectValues["x"] = new JsonObject(msg->GetStr("x"));
				player->objectValues["y"] = new JsonObject(msg->GetStr("y"));
				player->objectValues["s"] = new JsonObject("0");
				player->objectValues["r"] = new JsonObject("0");
				player->objectValues["ts"] = new JsonObject("0");
				player->objectValues["i"] = new JsonObject("nn");
				//player->objectValues["c"] = new JsonObject(msg->GetStr("color"));
				DEBUG(player->stringify())
				game_state["players"]->objectValues[msg->GetStr("handle") + "tank"] = player;
			}
			// ELSE do nothing because they have already spawned.
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

