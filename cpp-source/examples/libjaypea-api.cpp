#include "http-api.hpp"
#include "shell.hpp"

int main(int argc, char **argv){
	std::string public_directory;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int port = 443;
	bool http = false;
	int cache_megabytes = 30;
	std::string password = "abc123";

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("http", &http);
	Util::define_argument("cache_megabytes", &cache_megabytes, {"-cm"});
	Util::define_argument("password", password, {"-pw"});
	Util::parse_arguments(argc, argv, "This serves files over HTTP(S), and JSON (with or without HTTP headers) through an API.");

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
	
	Shell shell;
	if(shell.sopen()){
		ERROR("Shell failed.")
		return -1;
	}
	std::string pwd = Util::exec_and_wait("pwd");
	pwd.pop_back();
	std::string redeploy = pwd + "/scripts/extras/redeploy.sh 2>&1 | tee " + pwd + "/logs/redeploy.log\n";
	
	api.route("GET", "/", [&](JsonObject*)->std::string{
		return "{\"result\":\"Welcome to the API!\",\n\"routes\":" + api.routes_string + "}";
	});

	api.route("POST", "/build", [&](JsonObject* json)->std::string{
		if(json->GetStr("token") == password){
			PRINT("SHELL:" << redeploy)
			if(write(shell.input, redeploy.c_str(), redeploy.length()) != static_cast<ssize_t>(redeploy.length())){
				throw std::runtime_error("Couldn't write to shell.");
			}
			return "{\"result\":\"Building.\"}";
		}
		return std::string();
	}, {{"token", STRING}});

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

	api.route("GET", "/distribution", [&](JsonObject* json)->std::string{
		return "{\"result\":\"dist\"}";
	}, {{"token", STRING}});

	api.start();
}
