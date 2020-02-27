#include <limits>
#include <cmath>

#include <signal.h>

#include "http-api.hpp"
#include "pgsql-model.hpp"
#include "websocket-server.hpp"
#include "tls-websocket-server.hpp"

int main(int argc, char **argv){
	#include "jph2/initialization.cpp"
	
	#include "jph2/models.cpp"
	
	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the jph2.net API!\"}";
	});
	
	api.route("GET", "/routes", [&](JsonObject*)->std::string{
		return api.routes_string;
	});
	
	#include "jph2/users.cpp"
	
	api.route("POST", "/login", login, {{"username", STRING}, {"password", STRING}}, std::chrono::seconds(1));
	api.authenticated_route("POST", "/token", check_token);
	
	api.route("POST", "/user", create_user, {{"values", OBJECT}}, std::chrono::seconds(2));
	api.authenticated_route("POST", "/get/users", read_users);
	api.authenticated_route("PUT", "/user", update_user, {{"values", OBJECT}}, std::chrono::seconds(2));
	
	api.authenticated_route("POST", "/my/user", read_my_user);
	api.authenticated_route("POST", "/my/access", read_my_access);
	api.authenticated_route("POST", "/my/threads", read_my_threads);
	api.authenticated_route("POST", "/my/poi", read_my_poi);
	
	#include "jph2/threads.cpp"
	
	api.authenticated_route("POST", "/thread", create_thread, {{"values", OBJECT}}, std::chrono::seconds(2));
	api.route("GET", "/thread", read_thread, {{"id", STRING}});
	api.authenticated_route("PUT", "/thread", update_thread, {{"id", STRING}, {"values", OBJECT}}, std::chrono::seconds(2));
	api.authenticated_route("DELETE", "/thread", delete_thread, {{"id", STRING}});

	#include "jph2/messages.cpp"
	
	api.authenticated_route("POST", "/message", create_message, {{"values", OBJECT}}, std::chrono::seconds(2));
	api.route("GET", "/message", read_message, {{"id", STRING}});
	api.authenticated_route("PUT", "/message", update_message, {{"id", STRING}, {"values", OBJECT}}, std::chrono::seconds(2));
	api.authenticated_route("DELETE", "/message", delete_message, {{"id", STRING}});

	api.route("GET", "/thread/messages", read_thread_messages, {{"id", STRING}});
	api.route("GET", "/thread/messages/by/name", read_thread_messages_by_name, {{"name", STRING}});
	
	#include "jph2/poi.cpp"
	
	api.authenticated_route("POST", "/poi", create_poi, {{"values", OBJECT}}, std::chrono::seconds(2));
	api.route("GET", "/poi", read_poi, {{"id", STRING}});
	api.authenticated_route("PUT", "/poi", update_poi, {{"id", STRING}, {"values", OBJECT}}, std::chrono::seconds(2));
	api.authenticated_route("DELETE", "/poi", delete_poi, {{"id", STRING}});
	
	api.route("GET", "/poi/by/username", read_poi_by_username, {{"username", STRING}});
	
	#include "jph2/chat.cpp"
	
	api.route("GET", "/chat", read_chat);
	api.authenticated_route("POST", "/user/chat", create_authenticated_chat, {{"message", STRING}}, std::chrono::seconds(2));
	api.route("POST", "/chat", create_chat, {{"handle", STRING}, {"message", STRING}}, std::chrono::seconds(2));
	
	#include "jph2/http-redirecter.cpp"
	
	#include "jph2/tanks-wss.cpp"

	api.form_route("POST", "/contact.html", [&](JsonObject* json)->View{
		char response[PACKET_LIMIT];
		if(Util::https_client.post(443, "api.sendgrid.com", "/v3/mail/send",
		{{"Content-Type", "application/json"},
		{"Authorization", "Bearer " + sendgrid_key}},
		"{\"personalizations\": [{\"to\": [{\"email\": \"" + admin_email + "\"}]}],\"from\": {\"email\": \"" + admin_email + "\"},\"subject\": \"JPH2, " + json->GetURLDecodedStr("name") + "\",\"content\": [{\"type\": \"text/html\", \"value\": \"<h2>" + json->GetURLDecodedStr("email") + "</h2><br>" + json->GetURLDecodedStr("message") + "\"}]}",
		response)){
			PRINT("FAILED TO SEND EMAIL: " + json->GetStr("name") + ", " + json->GetStr("email") + ", " + json->GetStr("message"))
		}

		return View("/thanks.html");
	}, {{"name", STRING}, {"email", STRING}, {"captcha", STRING}, {"message", STRING}}, std::chrono::seconds(5), true);
	
	#if defined(DO_DEBUG)
		api.route("GET", "/end", [&](JsonObject*)->std::string{
			game_server.running = false;
			server.running = false;
			http_server.running = false;
			return "APPLICATION ENDING";
		});
	#endif
	
	api.authenticated_route("POST", "/backup", [&](JsonObject*, JsonObject* token)->std::string{
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

