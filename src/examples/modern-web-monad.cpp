#include "web-monad.hpp"

int main(int argc, char **argv){
	std::string public_directory;
	std::string hostname;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int http_port = 80;
	int https_port = 443;

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("hostname", hostname, {"-hn"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("http", &http_port);
	Util::define_argument("https", &https_port);
	Util::parse_arguments(argc, argv, "This is a modern web server monad which starts an HTTP redirection server, an HTTPS server for files, and a JSON API. Configured via etc/configuration.json.");

	WebMonad monad(hostname,
		Util::root_path + public_directory,
		Util::root_path + ssl_certificate,
		Util::root_path + ssl_private_key,
		static_cast<uint16_t>(http_port),
		static_cast<uint16_t>(https_port));

	monad.route("GET", "/", [&](JsonObject*)->std::string{
		JsonObject result(OBJECT);
		result.objectValues["result"] = new JsonObject("Welcome to the API!");
		result.objectValues["routes"] = monad.routes_object;
		return result.stringify(true);
		// Simpler.
		// return "{\"result\":\"Welcome to the API!\",\"routes\":\"" + monad.routes_string + "\"}";
	});

	std::mutex message_mutex;
	std::deque<std::string> messages;

	monad.route("GET", "/message", [&](JsonObject*)->std::string{
		JsonObject result(OBJECT);
		result.objectValues["result"] = new JsonObject(ARRAY);

		message_mutex.lock();
		for(auto it = messages.begin(); it != messages.end(); ++it){
			result["result"]->arrayValues.push_back(new JsonObject(*it));
		}
		message_mutex.unlock();

		return result.stringify();
	});

	monad.route("POST", "/message", [&](JsonObject* json)->std::string{
		message_mutex.lock();
		messages.push_front(json->objectValues["message"]->stringValue);
		if(messages.size() > 10){
			messages.pop_back();
		}
		message_mutex.unlock();

		return "{\"result\":\"Message posted.\"}";
	}, {{"message", STRING}});

	monad.start();
}
