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
	
	TlsEpollServer server(ssl_certificate, ssl_private_key, static_cast<uint16_t>(https_port), 10);
	SymmetricEncryptor encryptor;
	HttpApi api(public_directory, &server, &encryptor);
	api.set_file_cache_size(cache_megabytes);
	
	#include "jph2/models.cpp"
	
	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API V1!\",\n\"routes\":" + api.routes_string + "}";
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

