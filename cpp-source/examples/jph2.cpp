#include <limits>

#include <signal.h>

#include "http-api.hpp"
#include "pgsql-model.hpp"
#include "websocket-server.hpp"
#include "tls-websocket-server.hpp"

int main(int argc, char **argv){
	#include "jph2/initialization.cpp"
	
	#include "jph2/models.cpp"
	
	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API V1!\"}";
	});
	
	api.route("GET", "/routes", [&](JsonObject*)->std::string{
		return api.routes_string;
	});
	
	#include "jph2/users.cpp"
	
	api.route("POST", "/login", login, {{"username", STRING}, {"password", STRING}}, std::chrono::seconds(1));
	api.route("POST", "/token", check_token);
	
	api.route("POST", "/user", create_user, {{"values", OBJECT}}, std::chrono::hours(1));
	api.route("POST", "/get/users", read_users);
	api.route("PUT", "/user", update_user, {{"values", OBJECT}}, std::chrono::seconds(2));
	
	api.route("POST", "/my/user", read_my_user);
	api.route("POST", "/my/access", read_my_access);
	api.route("POST", "/my/threads", read_my_threads);
	api.route("POST", "/my/poi", read_my_poi);
	
	#include "jph2/threads.cpp"
	
	api.route("POST", "/thread", create_thread, {{"values", OBJECT}}, std::chrono::seconds(2));
	api.route("GET", "/thread", read_thread, {{"id", STRING}});
	api.route("PUT", "/thread", update_thread, {{"id", STRING}, {"values", OBJECT}}, std::chrono::seconds(2));
	api.route("DELETE", "/thread", delete_thread, {{"id", STRING}});

	#include "jph2/messages.cpp"
	
	api.route("POST", "/message", create_message, {{"values", OBJECT}}, std::chrono::seconds(2));
	api.route("GET", "/message", read_message, {{"id", STRING}});
	api.route("PUT", "/message", update_message, {{"id", STRING}, {"values", OBJECT}}, std::chrono::seconds(2));
	api.route("DELETE", "/message", delete_message, {{"id", STRING}});

	api.route("GET", "/thread/messages", read_thread_messages, {{"id", STRING}});
	api.route("GET", "/thread/messages/by/name", read_thread_messages_by_name, {{"name", STRING}});
	
	#include "jph2/poi.cpp"
	
	api.route("POST", "/poi", create_poi, {{"values", OBJECT}}, std::chrono::seconds(2));
	api.route("GET", "/poi", read_poi, {{"id", STRING}});
	api.route("PUT", "/poi", update_poi, {{"id", STRING}, {"values", OBJECT}}, std::chrono::seconds(2));
	api.route("DELETE", "/poi", delete_poi, {{"id", STRING}});
	
	api.route("GET", "/poi/by/username", read_poi_by_username, {{"username", STRING}});
	
	#include "jph2/chat.cpp"
	
	api.route("GET", "/chat", read_chat);
	api.route("POST", "/user/chat", create_authenticated_chat, {{"message", STRING}}, std::chrono::seconds(2));
	api.route("POST", "/chat", create_chat, {{"handle", STRING}, {"message", STRING}}, std::chrono::seconds(2));
	
	#include "jph2/http-redirecter.cpp"
	
	#include "jph2/tanks-wss.cpp"
	
	#if defined(DO_DEBUG)
		api.route("GET", "/end", [&](JsonObject*)->std::string{
			game_server.running = false;
			server.running = false;
			http_server.running = false;
			return "APPLICATION ENDING";
		});
	#endif
	
	api.route("POST", "/backup", [&](JsonObject*, JsonObject* token)->std::string{
		JsonObject* accesses = access->Where("owner_id", token->GetStr("id"));
		for(auto it = accesses->arrayValues.begin(); it != accesses->arrayValues.end(); ++it){
			if((*it)->GetStr("access_type_id") == "1"){
				return Util::exec_and_wait("pg_dump -U postgres webservice");
			}
		}
	
		return INSUFFICIENT_ACCESS;
	});
	
	api.start();
	
	delete users;
	delete poi;
	delete threads;
	delete messages;
	//delete access_types;
	delete access;
	
	Util::clean();
	
	PRINT("DONE!")
}

