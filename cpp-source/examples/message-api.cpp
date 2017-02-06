#include "https-api.hpp"

int main(int argc, char **argv){
	std::string public_directory;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int port = 443;

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("port", &port, {"-p"});
	Util::parse_arguments(argc, argv, "This serves files over HTTPS , and JSON (with or without HTTP headers) through an API.");

	HttpsApi server(public_directory, ssl_certificate, ssl_private_key, static_cast<uint16_t>(port));

	server.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API!\",\n\"routes\":" + server.routes_string + "}";
	});

	std::mutex message_mutex;
	std::deque<std::string> messages;

	server.route("GET", "/message", [&](JsonObject*)->std::string{
		JsonObject result(OBJECT);
		result.objectValues["result"] = new JsonObject(ARRAY);

		message_mutex.lock();
		for(auto it = messages.begin(); it != messages.end(); ++it){
			result["result"]->arrayValues.push_back(new JsonObject(*it));
		}
		message_mutex.unlock();

		return result.stringify();
	});

	server.route("POST", "/message", [&](JsonObject* json)->std::string{
		message_mutex.lock();
		messages.push_front(json->objectValues["message"]->stringValue);
		if(messages.size() > 10){
			messages.pop_back();
		}
		message_mutex.unlock();

		return "{\"result\":\"Message posted.\"}";
	}, {{"message", STRING}});

	server.start();
}
