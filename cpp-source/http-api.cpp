#include <random>

#include "http-api.hpp"

HttpApi::HttpApi(std::string new_public_directory, EpollServer* new_server)
:public_directory(new_public_directory),
server(new_server)
{
	// 30 MB cache size by default.
	this->set_file_cache_size(30);
}

void HttpApi::route(std::string method, std::string path, std::function<std::string(JsonObject*)> function, std::unordered_map<std::string, JsonType> requires, bool requires_human){
	if(path[path.length() - 1] != '/'){
		path += '/';
	}
	this->routemap[method + " /api" + path] = new Route(function, requires, requires_human);
}

void HttpApi::route(std::string method, std::string path, std::function<ssize_t(JsonObject*, int)> function, std::unordered_map<std::string, JsonType> requires, bool requires_human){
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

void HttpApi::start(){
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

	PRINT("HttpApi running with routes: " << this->routes_string)
	
	this->server->on_connect = [&](int fd){
		client_questions[fd] = get_question();
	};

	this->server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT("RECV:" << data)
		
		JsonObject r_obj(OBJECT);
		enum RequestResult r_type = Util::parse_http_api_request(data, &r_obj);

		std::string response_body = std::string();
		std::string response = std::string();
		
		if(r_type == JSON){
			char json_data[PACKET_LIMIT];
			
			auto get_body_callback = [&](int, const char*, size_t dl)->ssize_t{
				PRINT("GOT JSON:" << json_data);
				return static_cast<ssize_t>(dl);
			};
			
			if(this->server->recv(fd, json_data, PACKET_LIMIT, get_body_callback) <= 0){
				PRINT("FAILED TO GET JSON POST BODY");
				return -1;
			}
			
			r_obj.parse(json_data);
			
			r_type = HTTP_API;
		}
		
		PRINT("JSON:" << r_obj.stringify(true))
		
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
			
			if(this->file_cache.count(clean_route + "/index.html")){
				clean_route += "/index.html";
			}
			
			if(this->file_cache.count(clean_route)){
				// Send the file from the cache.
				CachedFile* cached_file = file_cache[clean_route];
				
				response = response_header + "Content-Length: " + std::to_string(cached_file->data_length) + "\n\n";
				if(this->server->send(fd, response.c_str(), response.length())){
					return -1;
				}
				PRINT("DELI:" << response)

				size_t buffer_size = BUFFER_LIMIT;
				size_t offset = 0;
				while(buffer_size == BUFFER_LIMIT){
					if(offset + buffer_size > cached_file->data_length){
						// Send final bytes.
						buffer_size = cached_file->data_length % BUFFER_LIMIT;
					}
					if(this->server->send(fd, cached_file->data + offset, buffer_size)){
						return -1;
					}
					offset += buffer_size;
				}
				PRINT("FILE CACHED |" << clean_route)
			}else{
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

					if(this->file_cache_remaining_bytes > route_stat.st_size && this->file_cache_mutex.try_lock()){
						// Stick the file into the cache AND send it
						CachedFile* cached_file = new CachedFile();
						cached_file->data_length = static_cast<size_t>(route_stat.st_size);
						cached_file->data = new char[route_stat.st_size];
						int offset = 0;
						this->file_cache[clean_route] = cached_file;
						this->file_cache_remaining_bytes -= cached_file->data_length;

						int file_fd;
						ssize_t len;
						char buffer[BUFFER_LIMIT];
						if((file_fd = open(clean_route.c_str(), O_RDONLY)) < 0){
							ERROR("open file")
							this->file_cache_mutex.unlock();
							return 0;
						}
						while(file_fd > 0){
							if((len = read(file_fd, buffer, BUFFER_LIMIT)) < 0){
								ERROR("read file")
							this->file_cache_mutex.unlock();
								return 0;
							}
							buffer[len] = 0;
							// Only different line (copy into memory)
							std::memcpy(cached_file->data + offset, buffer, static_cast<size_t>(len));
							offset += len;
							if(this->server->send(fd, buffer, static_cast<size_t>(len))){
								this->file_cache_mutex.unlock();
								return -1;
							}
							if(len < BUFFER_LIMIT){
								break;
							}
						}
						if(close(file_fd) < 0){
							ERROR("close file")
							this->file_cache_mutex.unlock();
							return 0;
						}
						PRINT("FILE DONE AND CACHED |" << clean_route)
						this->file_cache_mutex.unlock();
					}else{
						// Send the file read-buffer style
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
						PRINT("FILE DONE |" << clean_route)
					}
				}else{
					PRINT("Something other than a regular file was requested...")
					response_body = HTTP_404;
				}
			}
		}else{
			if(route.length() >= 4 &&
			!Util::strict_compare_inequal(route.c_str(), "/api", 4)){
				route = r_obj.GetStr("method") + ' ' + route;
				if(route[route.length() - 1] != '/'){
					route += '/';
				}
			}

			PRINT((r_type == API ? "APIR:" : "HTTPAPIR") << route)

			if(this->routemap.count(route)){
				for(auto iter = this->routemap[route]->requires.begin(); iter != this->routemap[route]->requires.end(); ++iter){
					if(!r_obj.HasObj(iter->first, iter->second)){
						response_body = "{\"error\":\"'" + iter->first + "' requires a " + JsonObject::typeString[iter->second] + ".\"}";
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
						if(this->routemap[route]->function != nullptr){
							response_body = this->routemap[route]->function(&r_obj);
						}else{
							if(this->routemap[route]->raw_function(&r_obj, fd) <= 0){
								PRINT("RAW FUNCTION BAD")
								return -1;
							}else{
								r_type = JSON;
							}
						}
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
		
		if(!response_body.empty() && r_type != JSON){
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

void HttpApi::set_file_cache_size(int megabytes){
	this->file_cache_remaining_bytes = megabytes * 1024 * 1024;
}

