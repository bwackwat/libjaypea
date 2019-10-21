
// TODO: This needs TLS because the page is loaded in TLS... Maybe make a special HTTP exception here.
// You can currently spoof other players, but you cant control the movement of players.
//WebsocketServer game_server(static_cast<uint16_t>(tanks_port), 10);
TlsWebsocketServer game_server(ssl_certificate, ssl_private_key, static_cast<uint16_t>(tanks_port), 10);

// Unlike the structure in TcpServer, this structure indicates "joined players".
// Need to build out a larger UI and corresponding events for players.
std::unordered_map<int, JsonObject*> client_details;

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
	uint8_t type;
	
	int fd;
	std::string player;
	
	Event(uint8_t ntype, int nfd, std::string nplayer): type(ntype), fd(nfd), player(nplayer){}
	Event(uint8_t ntype, std::string nplayer): type(ntype), player(nplayer){}
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

