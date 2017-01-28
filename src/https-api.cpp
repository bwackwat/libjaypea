#include <random>

#include "https-api.hpp"

HttpsApi::HttpsApi(std::string new_public_directory, std::string ssl_certificate, std::string ssl_private_key, uint16_t https_port)
:public_directory(new_public_directory),
server(new PrivateEpollServer(ssl_certificate, ssl_private_key, https_port, 10))
{}

void HttpsApi::route(std::string method, std::string path, std::function<std::string(JsonObject*)> function, std::unordered_map<std::string, JsonType> requires, bool requires_human){
	if(path[path.length() - 1] != '/'){
		path += '/';
	}
	this->routemap[method + " /api" + path] = new Route(function, requires, requires_human);
}

struct Question{
	std::string q;
	std::vector<std::string> a;
};

static struct Question questions[6] = {
	{"What is the basic color of the sky?", {"blue"}},
	{"What is the basic color of grass?", {"green"}},
	{"What is the basic color of blood?", {"red"}},
	{"What is the first name of the current president?", {"donald"}},
	{"What is the last name of the current president?", {"trump"}},
	{"How many planets are in the solar system?", {"8", "9"}}
};

static std::random_device rd;
static std::mt19937 mt(rd());
static std::uniform_int_distribution<size_t> dist(0, 5);

static struct Question* get_question(){
	return &questions[dist(mt)];
}

static std::unordered_map<int, struct Question*> client_questions;

void HttpsApi::start(){
	std::string response_header = "HTTP/1.1 200 OK\n"
		"Accept-Ranges: bytes\n"
		"Content-Type: text\n";

	this->routes_object = new JsonObject(OBJECT);
	for(auto iter = this->routemap.begin(); iter != this->routemap.end(); ++iter){
		routes_object->objectValues[iter->first] = new JsonObject(ARRAY);
		for(auto &field : iter->second->requires){
			routes_object->objectValues[iter->first]->arrayValues.push_back(new JsonObject(field.first));
		}
	}
	this->routes_string = routes_object->stringify(true);

	PRINT("WebMonad running with routes: " << this->routes_string)
	
	this->server->on_connect = [&](int fd){
		client_questions[fd] = get_question();
	};

	this->server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT(data)
		JsonObject r_obj(OBJECT);
		enum RequestResult r_type = Util::parse_http_api_request(data, &r_obj);
		PRINT("JSON:" << r_obj.stringify(true))

		std::string response_body = std::string();
		std::string response = std::string();
		std::string route = r_obj.GetStr("route");
		
		if(r_type == HTTP){
			std::string clean_route = this->public_directory;
			for(size_t i = 0; i < route.length(); ++i){
				if(i < route.length() - 1 &&
				route[i] == '.' &&
				route[i + 1] == '.'){
					continue;
				}
				clean_route += route[i];
			}

			struct stat route_stat;
			if(lstat(clean_route.c_str(), &route_stat) < 0){
				// perror("lstat");
				response_body = HTTP_404;
			}else if(S_ISDIR(route_stat.st_mode)){
				clean_route += "/index.html";
				if(lstat(clean_route.c_str(), &route_stat) < 0){
					// perror("lstat index.html");
					response_body = HTTP_404;
				}
			}
			
			if(response_body.empty() && S_ISREG(route_stat.st_mode)){
				response = response_header + "Content-Length: " + std::to_string(route_stat.st_size) + "\n\n";
				if(this->server->send(fd, response.c_str(), response.length())){
					return -1;
				}
				PRINT("DELI:" << response)

				int file_fd;
				ssize_t len;
				char buffer[BUFFER_LIMIT];
				if((file_fd = open(clean_route.c_str(), O_RDONLY)) < 0){
					ERROR("open file")
					return 0;
				}
				while(file_fd > 0){
					if((len = read(file_fd, buffer, BUFFER_LIMIT)) < 0){
						ERROR("read file")
						return 0;
					}
					buffer[len] = 0;
					if(this->server->send(fd, buffer, static_cast<size_t>(len))){
						return -1;
					}
					if(len < BUFFER_LIMIT){
						break;
					}
				}
				if(close(file_fd) < 0){
					ERROR("close file")
					return 0;
				}
				PRINT("FILE DONE")
			}else{
				PRINT("Something other than a regular file was requested...")
				response_body = HTTP_404;
			}
		}else{
			if(route.length() >= 4 &&
			!Util::strict_compare_inequal(route.c_str(), "/api", 4)){
				route = r_obj.GetStr("method") + ' ' + route;
				if(route[route.length() - 1] != '/'){
					route += '/';
				}
			}
			PRINT("APIR:" << route)

			if(this->routemap.count(route)){
				for(auto iter = this->routemap[route]->requires.begin(); iter != this->routemap[route]->requires.end(); ++iter){
					if(!r_obj.HasObj(iter->first, iter->second)){
						response_body = "{\"error\":\"'" + iter->first + "' requries a " + JsonObject::typeString[iter->second] + ".\"}";
						break;
					}
				}
				if(response_body.empty()){
					if(this->routemap[route]->requires_human){
						if(!r_obj.HasObj("answer", STRING)){
							response_body = "{\"error\":\"You need to answer the question: " + client_questions[fd]->q + "\"}";
						}else{
							for(auto sa : client_questions[fd]->a){
								if(r_obj.GetStr("answer") == sa){
									response_body = this->routemap[route]->function(&r_obj);
									client_questions[fd] = get_question();
									break;
								}
							}
							if(response_body.empty()){
								response_body = "{\"error\":\"You provided an incorrect answer.\"}";
							}
						}
					}else{
						response_body = this->routemap[route]->function(&r_obj);
					}
					if(response_body.empty()){
						response_body = "{\"error\":\"The data could not be acquired.\"}";
					}
				}
			}else if(route == "GET /api/question/"){
				response_body = "{\"result\":\"Human verification question: " + client_questions[fd]->q + "\"}";
			}else{
				response_body = "{\"error\":\"Invalid API route.\"}";
			}
		}
		
		if(!response_body.empty()){
			if(r_type == API){
				if(this->server->send(fd, response_body.c_str(), response_body.length())){
					return -1;
				}
				PRINT("DELI:" << response_body)
			}else{
				response = response_header + "Content-Length: " + std::to_string(response_body.length()) + "\n\n" + response_body;
				if(this->server->send(fd, response.c_str(), response.length())){
					return -1;
				}
				PRINT("DELI:" << response)
			}
		}
		
		return data_length;
	};

	// Should not return.
	this->server->run();

	ERROR("something super broke")
}
