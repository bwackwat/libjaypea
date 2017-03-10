#include <fstream>

#include "http-api.hpp"

int main(int argc, char **argv){
	std::string public_directory;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int port = 443;
	bool http = false;
	int cache_megabytes = 30;
	std::string password = "abc123";
	std::string services_path = "artifacts/host-services.json";

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("http", &http);
	Util::define_argument("cache_megabytes", &cache_megabytes, {"-cm"});
	Util::define_argument("password", password, {"-pw"});
	Util::define_argument("services_file", services_path, {"-sf"});
	Util::parse_arguments(argc, argv, "This serves files over HTTP(S), and JSON (with or without HTTP headers) through an API.");

	std::ifstream services_file(services_path);
	JsonObject services;
	if(services_file.is_open()){
		std::string services_data((std::istreambuf_iterator<char>(services_file)), (std::istreambuf_iterator<char>()));
		services.parse(services_data.c_str());
		PRINT("LOADED SERVICES FILE: " << services_path)
		PRINT(services.stringify(true));
	}

	EpollServer* server;
	if(http){
		server = new EpollServer(static_cast<uint16_t>(port), 10);
	}else{
		server = new TlsEpollServer(ssl_certificate, ssl_private_key, static_cast<uint16_t>(port), 10);
	}
	
	HttpApi api(public_directory, server);
	api.set_file_cache_size(cache_megabytes);

	std::mutex message_mutex;
	std::deque<std::string> messages;
	
	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API! V1\",\n\"routes\":" + api.routes_string + "}";
	});

	api.route("GET", "/message", [&](JsonObject*)->std::string{
		JsonObject result(OBJECT);
		result.objectValues["result"] = new JsonObject(ARRAY);

		message_mutex.lock();
		for(auto it = messages.begin(); it != messages.end(); ++it){
			result["result"]->arrayValues.push_back(new JsonObject(*it));
		}
		message_mutex.unlock();

		return result.stringify();
	});

	api.route("POST", "/message", [&](JsonObject* json)->std::string{
		message_mutex.lock();
		messages.push_front(json->objectValues["message"]->stringValue);
		if(messages.size() > 100){
			messages.pop_back();
		}
		message_mutex.unlock();

		return "{\"result\":\"Message posted.\"}";
	}, {{"message", STRING}});

	api.route("GET", "/host-service", [&](JsonObject* json)->std::string{
		if(json->GetStr("token") != password){
			return std::string();
		}
		if(services.HasObj(json->GetStr("host"), OBJECT) &&
		services.objectValues[json->GetStr("host")]->objectValues.count(json->GetStr("service"))){
			return services.objectValues[json->GetStr("host")]->objectValues[json->GetStr("service")]->stringify(true);
		}
		return std::string();
	}, {{"token", STRING}, {"host", STRING}, {"service", STRING}});

	api.start();
}
