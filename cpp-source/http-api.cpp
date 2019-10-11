#include <random>
#include <regex>

#include "http-api.hpp"

HttpApi::HttpApi(std::string new_public_directory, EpollServer* new_server, SymmetricEncryptor* new_encryptor)
:public_directory(new_public_directory),
server(new_server),
encryptor(new_encryptor)
{
	this->server->set_timeout(10);
}

HttpApi::HttpApi(std::string new_public_directory, EpollServer* new_server)
:public_directory(new_public_directory),
server(new_server),
encryptor(0)
{
	this->server->set_timeout(10);
}

void HttpApi::form_route(std::string method, std::string path, std::function<View(JsonObject*)> function,
std::unordered_map<std::string, JsonType> requires,
std::chrono::milliseconds rate_limit,
bool requires_human){
	this->routemap[method + " " + path] = new Route(function, requires, rate_limit, requires_human);
}

void HttpApi::route(std::string method, std::string path, std::function<std::string(JsonObject*)> function,
std::unordered_map<std::string, JsonType> requires,
std::chrono::milliseconds rate_limit,
bool requires_human){
	if(path[path.length() - 1] != '/'){
		path += '/';
	}
	this->routemap[method + " /api" + path] = new Route(function, requires, rate_limit, requires_human);
}

void HttpApi::authenticated_route(std::string method, std::string path, std::function<std::string(JsonObject*, JsonObject*)> function,
std::unordered_map<std::string, JsonType> requires,
std::chrono::milliseconds rate_limit,
bool requires_human){
	if(encryptor == 0){
		PRINT("You tried to add a route that uses an encrypted token. Please provide an encryptor in the HttpApi constructor.")
		exit(1);
	}
	if(path[path.length() - 1] != '/'){
		path += '/';
	}
	this->routemap[method + " /api" + path] = new Route(function, requires, rate_limit, requires_human);
}

void HttpApi::raw_route(std::string method, std::string path, std::function<ssize_t(JsonObject*, int)> function,
std::unordered_map<std::string, JsonType> requires,
std::chrono::milliseconds rate_limit,
bool requires_human){
	if(path[path.length() - 1] != '/'){
		path += '/';
	}
	this->routemap[method + " /api" + path] = new Route(function, requires, rate_limit, requires_human);
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

ssize_t HttpApi::respond(int fd, HttpResponse* response){
	response->headers["Content-Length"] = std::to_string(response->body.length());
	return this->server->send(fd, response->str());
}

ssize_t HttpApi::respond_404(int fd){
	std::string basic_404_html = "<h1 style='text-align:center;'>404 Not Found</h1>";
	HttpResponse response("404 Not Found", {{"Location", "404.html"}}, basic_404_html); 
	return this->respond(fd, &response);
}

ssize_t HttpApi::respond_cached_file(int fd, HttpResponse* response, CachedFile* cached_file){
	ssize_t headers_size = this->respond(fd, response);
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
	return headers_size + cached_file->data_length;
}

ssize_t HttpApi::respond_parameterized_view(int fd, View* view, HttpResponse* response){
	int file_fd;
	ssize_t len;
	char buffer[BUFFER_LIMIT + 32];

	if((file_fd = open(view->route.c_str(), O_RDONLY | O_NOFOLLOW)) < 0){
		perror("open file");
		return -1;
	}

	// Only "small html" should be "parameterized".
	if((len = read(file_fd, buffer, BUFFER_LIMIT)) < 0){
		perror("read file");
		return -1;
	}

	if(close(file_fd) < 0){
		perror("close file");
		return -1;
	}

	if(len != view->size){
		PRINT("COULDN'T READ WHOLE FILE SIZE")
		return -1;
	}

	// Make parameter substitutions.
	response->body = std::string(buffer, len);
	for(auto iter = view->parameters.begin(); iter != view->parameters.end(); ++iter){
		Util::replace_all(response->body, '{' + iter->first + '}', iter->second);
	}

	return this->respond(fd, response);
}

ssize_t HttpApi::respond_view(int fd, View* view, HttpResponse* response){
	int file_fd;
	ssize_t len;
	char buffer[BUFFER_LIMIT + 32];

	// Send headers.
	if(this->respond(fd, response) < 0){
		perror("send headers");
		return -1;
	}

	if((file_fd = open(view->route.c_str(), O_RDONLY | O_NOFOLLOW)) < 0){
		perror("open file");
		return -1;
	}

	// Send all file data without parameterization.
	while(file_fd > 0){
		if((len = read(file_fd, buffer, BUFFER_LIMIT)) < 0){
			perror("read file");
			return -1;
		}
		buffer[len] = 0;
		if(this->server->send(fd, buffer, static_cast<size_t>(len))){
			perror("send filepacket")
			return -1;
		}
		// Not complete packet, no more to read.
		if(len < BUFFER_LIMIT){
			break;
		}
	}

	if(close(file_fd) < 0){
		perror("close file");
		return -1;
	}

	return response->str().length() + view->size;
}

void HttpApi::start(void){
	this->routes_object = new JsonObject(OBJECT);
	for(auto iter = this->routemap.begin(); iter != this->routemap.end(); ++iter){
		routes_object->objectValues[iter->first] = new JsonObject(OBJECT);
		routes_object->objectValues[iter->first]->objectValues["rate_limit"] =
			new JsonObject(std::to_string(iter->second->minimum_ms_between_call.count()));
		routes_object->objectValues[iter->first]->objectValues["parameters"] = new JsonObject(ARRAY);
		
		for(auto &field : iter->second->requires){
			routes_object->objectValues[iter->first]->objectValues["parameters"]->arrayValues.push_back(new JsonObject(field.first));
		}
		if(iter->second->token_function != nullptr){
			routes_object->objectValues[iter->first]->objectValues["parameters"]->arrayValues.push_back(new JsonObject("token"));
		}
	}
	this->routes_string = routes_object->stringify();
	
	this->server->on_connect = [&](int fd){
		client_questions[fd] = get_question();
	};

	this->server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		JsonObject request_object(OBJECT);
		enum RequestResult request_type = Util::parse_http_request(data, &request_object);
		
		View view;
		HttpResponse response;








		if(request_type == HTTP){
			view.route = request_object.GetStr("route");
			std::string clean_route = this->public_directory;
			for(size_t i = 0; i < view.route.length(); ++i){
				if(i < view.route.length() - 1 &&
				view.route[i] == '.' &&
				view.route[i + 1] == '.'){
					continue;
				}
				clean_route += view.route[i];
			}

			struct stat route_stat;
			if(lstat(clean_route.c_str(), &route_stat) < 0){
				DEBUG("lstat error")
				return this->respond_404(fd);
			}
			
			if(S_ISDIR(route_stat.st_mode)){
				clean_route += "/index.html";
				if(lstat(clean_route.c_str(), &route_stat) < 0){
					DEBUG("lstat index.html error")
					return this->respond_404(fd);
				}
			}

			if(!S_ISREG(route_stat.st_mode)){
				PRINT("Something other than a regular file was requested...")
				return this->respond_404(fd);
			}

			bool is_html = Util::endsWith(clean_route, ".html");
			if(Util::endsWith(clean_route, ".css")){
				response.headers["Content-Type"] = "text/css";
			}else if(is_html){
				response.headers["Content-Type"] = "text/html";
			}else if(Util::endsWith(clean_route, ".svg")){
				response.headers["Content-Type"] = "image/svg+xml";
			}else if(Util::endsWith(clean_route, ".js")){
				response.headers["Content-Type"] = "text/javascript";
			}else{
				response.headers["Content-Type"] = "text/plain";
			}

			// Send regular file size for HEAD.
			if(request_object.GetStr("method") == "HEAD"){
				response.headers["Content-Length"] = std::to_string(route_stat.st_size);
				return this->respond(fd, &response);
			}

			// If the modified time changed, clear the cache.
			if(this->file_cache.count(clean_route) && route_stat.st_mtime != this->file_cache[clean_route]->modified){
				this->file_cache.erase(clean_route);
			}

			// Cache the file.
			if(!this->file_cache.count(clean_route) && this->file_cache_remaining_bytes > route_stat.st_size && this->file_cache_mutex.try_lock()){
				CachedFile* cached_file = new CachedFile();
				cached_file->data_length = static_cast<size_t>(route_stat.st_size);
				DEBUG("Caching " << clean_route << " for " << route_stat.st_size << " bytes.")
				cached_file->data = new char[route_stat.st_size];
				cached_file->modified = route_stat.st_mtime;
				this->file_cache[clean_route] = cached_file;
				this->file_cache_remaining_bytes -= cached_file->data_length;

				int file_fd, offset = 0;
				ssize_t len;
				char buffer[BUFFER_LIMIT + 32];
				if((file_fd = open(clean_route.c_str(), O_RDONLY | O_NOFOLLOW)) < 0){
					perror("open caching file");
					this->file_cache_mutex.unlock();
					return -1;
				}
				while(file_fd > 0){
					if((len = read(file_fd, buffer, BUFFER_LIMIT)) < 0){
						perror("read caching file");
						this->file_cache_mutex.unlock();
						return -1;
					}
					buffer[len] = 0;
					std::memcpy(cached_file->data + offset, buffer, static_cast<size_t>(len));
					offset += len;
					if(len < BUFFER_LIMIT){
						break;
					}
				}
				if(close(file_fd) < 0){
					perror("close caching file");
					this->file_cache_mutex.unlock();
					return -1;
				}
				PRINT("File cached: " << clean_route)
				this->file_cache_mutex.unlock();
			}

			// Send the file from the cache.
			if(this->file_cache.count(clean_route)){
				CachedFile* cached_file = file_cache[clean_route];
				
				// Only templatize if single packet .html.
				if(cached_file->data_length < BUFFER_LIMIT && is_html){
					// Make parameter substitutions.
					response.body = std::string(cached_file->data, cached_file->data_length);
					for(auto iter = view.parameters.begin(); iter != view.parameters.end(); ++iter){
						Util::replace_all(response.body, '{' + iter->first + '}', iter->second);
					}

					return respond(fd, &response);
				}else{
					return respond_cached_file(fd, &response, cached_file);
				}
				DEBUG("Cached file served: " << clean_route << " with " << cached_file->data_length << " bytes as " << response.headers["Content-Type"])
			}else{
				view.size = route_stat.st_size;
				if(view.size < BUFFER_LIMIT && is_html){
					// Send a single parameterized packet.
					return respond_parameterized_view(fd, &view, &response);
				}else{
					return respond_view(fd, &view, &response);
				}
			}










		if(request_type == HTTP_FORM){
			view = this->routemap[request_object.GetStr("method") + ' ' + request_object.GetStr("route")]->form_function(&request_object);
			request_type = HTTP;
		}else if(request_type == JSON){
			char json_data[PACKET_LIMIT + 32];
			
			auto get_body_callback = [&](int, const char*, size_t dl)->ssize_t{
				DEBUG("GOT JSON:" << json_data);
				return static_cast<ssize_t>(dl);
			};
			
			if(this->server->recv(fd, json_data, PACKET_LIMIT, get_body_callback) <= 0){
				PRINT("FAILED TO GET JSON POST BODY");
				return -1;
			}
			
			request_object.parse(json_data);
			request_type = HTTP_API;

			view.route = request_object.GetStr("route");
		}else{
			view.route = request_object.GetStr("route");
		}
		
		if(request_type == HTTP){
			std::string clean_route = this->public_directory;
			for(size_t i = 0; i < view.route.length(); ++i){
				if(i < view.route.length() - 1 &&
				view.route[i] == '.' &&
				view.route[i + 1] == '.'){
					continue;
				}
				clean_route += view.route[i];
			}
			
			if(this->file_cache.count(clean_route + "/index.html")){
				clean_route += "/index.html";
			}

			struct stat route_stat;
			if(lstat(clean_route.c_str(), &route_stat) < 0){
				DEBUG("lstat error")
				response.status = "404 Not Found";
				response.headers["Location"] = "/404.html";
				response.body = HTTP_404;
			}else if(S_ISDIR(route_stat.st_mode)){
				clean_route += "/index.html";
				if(lstat(clean_route.c_str(), &route_stat) < 0){
					DEBUG("lstat index.html error")
					response.status = "404 Not Found";
					response.headers["Location"] = "/404.html";
					response.body = HTTP_404;
				}
			}

			if(response.body.empty() && S_ISREG(route_stat.st_mode)){
				if(this->file_cache.count(clean_route) && route_stat.st_mtime != this->file_cache[clean_route]->modified){
					this->file_cache.erase(clean_route);
				}

				bool is_html = Util::endsWith(clean_route, ".html");
				if(Util::endsWith(clean_route, ".css")){
					response.headers["Content-Type"] = "text/css";
				}else if(is_html){
					response.headers["Content-Type"] = "text/html";
				}else if(Util::endsWith(clean_route, ".svg")){
					response.headers["Content-Type"] = "image/svg+xml";
				}else if(Util::endsWith(clean_route, ".js")){
					response.headers["Content-Type"] = "text/javascript";
				}else{
					response.headers["Content-Type"] = "text/plain";
				}

				if(this->file_cache.count(clean_route)){
					// Send the file from the cache.
					CachedFile* cached_file = file_cache[clean_route];
					
					if(request_object.GetStr("method") != "HEAD"){
						// Only templatize if single packet .html.
						if(cached_file->data_length < BUFFER_LIMIT && is_html){
							// Make parameter substitutions.
							response.body = std::string(cached_file->data, cached_file->data_length);
							for(auto iter = view.parameters.begin(); iter != view.parameters.end(); ++iter){
								Util::replace_all(response.body, '{' + iter->first + '}', iter->second);
							}

							// Send entire response with updated length.
							response.headers["Content-Length"] = std::to_string(response.body.length());
							std::string result = response.str();
							response.body.clear();
							if(this->server->send(fd, result.c_str(), result.length())){
								return -1;
							}
						}else{
							// Send headers.
							response.headers["Content-Length"] = std::to_string(cached_file->data_length);
							std::string result = response.str();
							response.body.clear();
							if(this->server->send(fd, result.c_str(), result.length())){
								return -1;
							}

							// Send buffered data without template substitution.
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
						}
					}
					DEBUG("Cached file served: " << clean_route << " with " << cached_file->data_length << " bytes as " << response.headers["Content-Type"])
				}else{
					// Only send headers if HEAD.
					if(request_object.GetStr("method") == "HEAD"){
						response.headers["Content-Length"] = std::to_string(route_stat.st_size);
						std::string result = response.str();
						response.body.clear();
						if(this->server->send(fd, result.c_str(), result.length())){
							return -1;
						}
					}else{
						CachedFile* cached_file = 0;
						int offset = 0;
						
						// If cache is big enouch and ready, stick the file into the cache.
						if(this->file_cache_remaining_bytes > route_stat.st_size && this->file_cache_mutex.try_lock()){
							cached_file = new CachedFile();
							cached_file->data_length = static_cast<size_t>(route_stat.st_size);
							DEBUG("Caching " << clean_route << " for " << route_stat.st_size << " bytes.")
							cached_file->data = new char[route_stat.st_size];
							cached_file->modified = route_stat.st_mtime;
							this->file_cache[clean_route] = cached_file;
							this->file_cache_remaining_bytes -= cached_file->data_length;
						}

						// If it' not small .html, data won't be templatized. Send headers.
						if(route_stat.st_size >= BUFFER_LIMIT || !is_html){
							response.headers["Content-Length"] = std::to_string(route_stat.st_size);
							std::string result = response.str();
							response.body.clear();
							if(this->server->send(fd, result.c_str(), result.length())){
								return -1;
							}
						}

						int file_fd;
						ssize_t len;
						char buffer[BUFFER_LIMIT + 32];
						if((file_fd = open(clean_route.c_str(), O_RDONLY | O_NOFOLLOW)) < 0){
							perror("open file");
							if(cached_file != 0){
								this->file_cache_mutex.unlock();
							}
							return 0;
						}
						while(file_fd > 0){
							if((len = read(file_fd, buffer, BUFFER_LIMIT)) < 0){
								perror("read file");
								if(cached_file != 0){
									this->file_cache_mutex.unlock();
								}
								return 0;
							}
							buffer[len] = 0;

							// Cache the data packet.
							if(cached_file != 0){
								std::memcpy(cached_file->data + offset, buffer, static_cast<size_t>(len));
								offset += len;
							}

							// If small .html, templatize and send
							if(route_stat.st_size < BUFFER_LIMIT && is_html){
								// Make parameter substitutions.
								response.body = std::string(cached_file->data, cached_file->data_length);
								for(auto iter = view.parameters.begin(); iter != view.parameters.end(); ++iter){
									Util::replace_all(response.body, '{' + iter->first + '}', iter->second);
								}

								// Send headers with updated length.
								response.headers["Content-Length"] = std::to_string(response.body.length());
								std::string result = response.str();
								response.body.clear();
								if(this->server->send(fd, result.c_str(), result.length())){
									if(cached_file != 0){
										this->file_cache_mutex.unlock();
									}
									return -1;
								}
							}else{
								if(this->server->send(fd, buffer, static_cast<size_t>(len))){
									if(cached_file != 0){
										this->file_cache_mutex.unlock();
									}
									return -1;
								}
							}
							
							// This must happen if data is templatized or else a double-send will occur.
							if(len < BUFFER_LIMIT){
								break;
							}
						}
						if(close(file_fd) < 0){
							perror("close file");
							if(cached_file != 0){
								this->file_cache_mutex.unlock();
							}
							return 0;
						}
						PRINT("File cached and served: " << clean_route)
						if(cached_file != 0){
							this->file_cache_mutex.unlock();
						}
					}
				}
			}else{
				PRINT("Something other than a regular file was requested...")
				response.status = "404 Not Found";
				response.headers["Location"] = "/404.html";
				response.body = HTTP_404;
			}
		}else{
			if(view.route.length() >= 4 &&
			!Util::strict_compare_inequal(view.route.c_str(), "/api", 4)){
				view.route = request_object.GetStr("method") + ' ' + view.route;
				if(view.route[view.route.length() - 1] != '/'){
					view.route += '/';
				}
			}

			if(request_type == API){
				PRINT("API ROUTE: " << view.route)
			}else if(request_type == HTTP_API){
				PRINT("HTTP API ROUTE: " << view.route)
			}else if(request_type == HTTP_FORM){
				PRINT("HTTP FORM ROUTE: " << view.route)
				view.route = request_object.GetStr("method") + ' ' + view.route;
			}else{
				PRINT("UNKNOWN ROUTE: " << view.route)
			}

			DEBUG(view.route)
			if(this->routemap.count(view.route)){
				if(this->routemap[view.route]->minimum_ms_between_call.count() > 0){
					std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::system_clock::now().time_since_epoch());

					if(this->routemap[view.route]->client_ms_at_call.count(this->server->fd_to_details_map[fd])){
						std::chrono::milliseconds diff = now -
						this->routemap[view.route]->client_ms_at_call[this->server->fd_to_details_map[fd]];

						if(diff < this->routemap[view.route]->minimum_ms_between_call){
							DEBUG("DIFF: " << diff.count() << "\nMINIMUM: " << this->routemap[view.route]->minimum_ms_between_call.count())
							response.body = "{\"error\":\"This route is rate-limited.\"}";
						}else{
							this->routemap[view.route]->client_ms_at_call[this->server->fd_to_details_map[fd]] = now;
						}
					}else{
						this->routemap[view.route]->client_ms_at_call[this->server->fd_to_details_map[fd]] = now;
					}
				}

				if(response.body.empty()){
					for(auto iter = this->routemap[view.route]->requires.begin(); iter != this->routemap[view.route]->requires.end(); ++iter){
						if(!request_object.HasObj(iter->first, iter->second)){
							response.body = "{\"error\":\"'" + iter->first + "' requires a " + JsonObject::typeString[iter->second] + ".\"}";
							break;
						}else{
							DEBUG("\t" << iter->first << ": " << request_object.objectValues[iter->first]->stringify(true))
						}
					}
				}

				if(response.body.empty()){
					if(this->routemap[view.route]->requires_human){
						if(!request_object.HasObj("answer", STRING)){
							response.body = "{\"error\":\"You need to answer the human verification question: " + client_questions[fd]->q + "\"}";
						}else{
							for(auto sa : client_questions[fd]->a){
								if(request_object.GetStr("answer") == sa){
									response.body = this->routemap[view.route]->function(&request_object);
									client_questions[fd] = get_question();
									break;
								}
							}
							if(response.body.empty()){
								response.body = "{\"error\":\"You provided an incorrect answer to the human verification question.\"}";
							}
						}
					}else{
						if(this->routemap[view.route]->function != nullptr){
							response.body = this->routemap[view.route]->function(&request_object);
						}else if(this->routemap[view.route]->token_function != nullptr){
							if(!request_object.HasObj("token", STRING)){
								response.body = "{\"error\":\"'token' requires a string.\"}";
							}else{
								JsonObject* token = new JsonObject();
								try{
									token->parse(this->encryptor->decrypt(JsonObject::deescape(request_object.GetStr("token"))).c_str());
									response.body = this->routemap[view.route]->token_function(&request_object, token);
								}catch(const std::exception& e){
									DEBUG(e.what())
									response.body = INSUFFICIENT_ACCESS;
								}
							}
						}else{
							if(this->routemap[view.route]->raw_function(&request_object, fd) <= 0){
								response.body = "{\"error\":\"The data could not be acquired.\"}";
							}
							request_type = JSON;
						}
						if(response.body.empty()){
							response.body = "{\"error\":\"The data could not be acquired.\"}";
						}
					}
				}
			}else if(view.route == "GET /api/question/"){
				response.body = "{\"result\":\"" + client_questions[fd]->q + "\"}";
			}else{
				PRINT("BAD ROUTE: " + view.route)
				response.body = "{\"error\":\"Invalid API route.\"}";
			}
		}
		
		if(!response.body.empty() && request_type != JSON){
			if(request_type == API){
				if(this->server->send(fd, response.body.c_str(), response.body.length())){
					return -1;
				}
			}else{
				if(request_type == HTTP){
					if(Util::endsWith(view.route, ".css")){
						response.headers["Content-Type"] = "text/css";
					}else if(Util::endsWith(view.route, ".svg")){
						response.headers["Content-Type"] = "image/svg+xml";
					}else{
						response.headers["Content-Type"] = "text/html";
					}
				}else{
					response.headers["Content-Type"] = "application/json";
				}
				response.headers["Content-Length"] = std::to_string(response.body.length());
				std::string result = response.str();
				if(this->server->send(fd, result.c_str(), result.length())){
					return -1;
				}
			}
		}
		
		return data_length;
	};

	// Now run forever.

	// As many threads as possible.
	this->server->run();
	
	// Single threaded.
	//this->server->run(false, 1);

	PRINT("Goodbye!")
}

void HttpApi::set_file_cache_size(int megabytes){
	this->file_cache_remaining_bytes = megabytes * 1024 * 1024;
}

HttpApi::~HttpApi(){
	delete this->routes_object;
	for(auto iter = this->routemap.begin(); iter != this->routemap.end(); ++iter){
		delete iter->second;
	}
	for(auto iter = this->file_cache.begin(); iter != this->file_cache.end(); ++iter){
		delete[] iter->second->data;
		delete iter->second;
	}
	DEBUG("API DELETED")
}
