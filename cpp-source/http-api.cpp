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

static std::random_device rd;
static std::mt19937 mt(rd());
static const char captcha_set[] =
        "23456789$@!=*"
        "ABCDEFGHJKMNPQRSTUVWXYZ"
        "abcdefghijkmnpqrstuvwxyz";
static std::uniform_int_distribution<size_t> distribution(0, sizeof(captcha_set) - 2);
static std::string get_captcha(){
	std::string captcha = "";
	for(int i = 0; i < 6; i++){
		captcha += captcha_set[distribution(mt)];
	}
	return captcha;
}

ssize_t HttpApi::respond(int fd, HttpResponse* response){
	if(!response->headers.count("Content-Length")){
		response->headers["Content-Length"] = std::to_string(response->body.length());
	}
	return this->server->send(fd, response->str());
}

ssize_t HttpApi::respond_404(int fd){
	std::string basic_404_html = "<h1 style='text-align:center;'>404 Not Found</h1>";
	HttpResponse response("404 Not Found", {{"Location", "404.html"}}, basic_404_html); 
	return this->respond(fd, &response);
}

ssize_t HttpApi::respond_cached_file(int fd, HttpResponse* response, CachedFile* cached_file){
	response->headers["Content-Length"] = std::to_string(cached_file->data_length);
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
	return headers_size + static_cast<ssize_t>(cached_file->data_length);
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
	response->body = std::string(buffer, static_cast<unsigned long>(len));
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
			perror("send filepacket");
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

	return static_cast<ssize_t>(response->str().length()) + view->size;
}

ssize_t HttpApi::respond_http(int fd, View* view, HttpResponse* response){
	std::string clean_route = this->public_directory;
	for(size_t i = 0; i < view->route.length(); ++i){
		if(i < view->route.length() - 1 &&
		view->route[i] == '.' &&
		view->route[i + 1] == '.'){
			continue;
		}
		clean_route += view->route[i];
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
		response->headers["Content-Type"] = "text/css";
	}else if(is_html){
		response->headers["Content-Type"] = "text/html";
	}else if(Util::endsWith(clean_route, ".svg")){
		response->headers["Content-Type"] = "image/svg+xml";
	}else if(Util::endsWith(clean_route, ".js")){
		response->headers["Content-Type"] = "text/javascript";
	}else{
		response->headers["Content-Type"] = "text/plain";
	}

	// Send regular file size for HEAD.
	if(view->method == "HEAD"){
		response->headers["Content-Length"] = std::to_string(route_stat.st_size);
		return this->respond(fd, response);
	}

	// If the modified time changed, clear the cache.
	if(this->file_cache.count(clean_route) && route_stat.st_mtime != this->file_cache[clean_route]->modified){
		DEBUG("Erase " << clean_route)
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
			response->body = std::string(cached_file->data, cached_file->data_length);
			for(auto iter = view->parameters.begin(); iter != view->parameters.end(); ++iter){
				Util::replace_all(response->body, '{' + iter->first + '}', iter->second);
			}

			return respond(fd, response);
		}else{
			DEBUG("Cached file served: " << clean_route << " with " << cached_file->data_length << " bytes as " << response->headers["Content-Type"])
			return respond_cached_file(fd, response, cached_file);
		}
	}else{
		view->size = route_stat.st_size;
		if(view->size < BUFFER_LIMIT && is_html){
			// Send a single parameterized packet.
			return respond_parameterized_view(fd, view, response);
		}else{
			return respond_view(fd, view, response);
		}
	}
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

	this->server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		JsonObject request_object(OBJECT);
		View view;
		HttpResponse response;
		enum RequestResult request_type = Util::parse_http_request(data, &request_object);

		//DEBUG(request_object.stringify(true))


		Session* session;

		// If the session key is bad or if it has expired (1 hour).
		if(request_object.HasObj("libjaypea-session", STRING)){
			if(!this->sessions.count(request_object.GetStr("libjaypea-session"))){
				request_object.objectValues.erase("libjaypea-session");
			}else if(std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()) - this->sessions[request_object.GetStr("libjaypea-session")]->created
			> std::chrono::seconds(3600)){
				request_object.objectValues.erase("libjaypea-session");
			}
		}

		if(!request_object.HasObj("libjaypea-session", STRING)){
			std::string input = this->server->fd_to_details_map[fd];
			if(request_object.HasObj("User-Agent", STRING)){
				input += request_object.GetStr("User-Agent");
			}
			if(request_object.HasObj("Host", STRING)){
				input += request_object.GetStr("Host");
			}
			request_object.objectValues["libjaypea-session"] = new JsonObject(Util::sha256_hash(input));
		}

		response.headers["libjaypea-session"] = request_object.GetStr("libjaypea-session");
		if(!this->sessions.count(response.headers["libjaypea-session"])){
			this->sessions[response.headers["libjaypea-session"]] = new Session();
		}

		session = this->sessions[response.headers["libjaypea-session"]];


		if(request_object.HasObj("method", STRING)){
			view.method = request_object.GetStr("method");
		}
		if(request_object.HasObj("route", STRING)){
			view.route = request_object.GetStr("route");
		}
		std::string route_key = "GET /404.html";
		if(request_object.HasObj("method", STRING) && request_object.HasObj("route", STRING)){
			route_key = request_object.GetStr("method") + ' ' + request_object.GetStr("route");
		}

		if(request_type == HTTP_API || request_type == API){
			if(route_key[route_key.length() - 1] != '/'){
				route_key += '/';
			}
		}

		if(request_type != HTTP){
			if(route_key == "GET /api/captcha/"){
				if(request_object.HasObj("new", STRING)){
					if(request_object.GetStr("new") == "true"){
						session->captcha.clear();
					}
				}
				if(session->captcha.empty()){
					session->captcha = get_captcha();
				}
				if(Util::secret_captcha_url.empty()){
					response.body = "{\"result\":\"<h3>" + session->captcha + "</h3>\"}";
				}else{
					response.body = Util::https_client.get(Util::secret_captcha_url + "&secret=" + session->captcha);
				}
				return respond(fd, &response);
			}

			if(!this->routemap.count(route_key)){
				if(request_type == HTTP_FORM){
					view.parameters["status"] = "Bad route.";
					return respond_http(fd, &view, &response);
				}
				response.body = "{\"error\":\"Bad route.\"}";
				return respond(fd, &response);
			}

			// For all but regular HTTP requests, check millseconds since last call by IP address.
			if(this->routemap[route_key]->minimum_ms_between_call.count() > 0){
				std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch());

				if(this->routemap[route_key]->client_ms_at_call.count(this->server->fd_to_details_map[fd])){
					std::chrono::milliseconds diff = now -
					this->routemap[route_key]->client_ms_at_call[this->server->fd_to_details_map[fd]];

					#if not defined(DO_DEBUG)
						if(diff < this->routemap[route_key]->minimum_ms_between_call){
							DEBUG("DIFF: " << diff.count() << "\nMINIMUM: " << this->routemap[route_key]->minimum_ms_between_call.count())
							if(request_type == HTTP_FORM){
								view.parameters["status"] = "This route is rate-limited.";
								return respond_http(fd, &view, &response);
							}
							response.body = "{\"error\":\"This route is rate-limited.\"}";
							return respond(fd, &response);
						}
					#endif
				}
				this->routemap[route_key]->client_ms_at_call[this->server->fd_to_details_map[fd]] = now;
			}

			for(auto iter = this->routemap[route_key]->requires.begin(); iter != this->routemap[route_key]->requires.end(); ++iter){
				if(!request_object.HasObj(iter->first, iter->second)){
					if(request_type == HTTP_FORM){
						view.parameters["status"] = "'" + iter->first + "' requires a " + JsonObject::typeString[iter->second] + ".";
						return respond_http(fd, &view, &response);
					}
					response.body = "{\"error\":\"'" + iter->first + "' requires a " + JsonObject::typeString[iter->second] + ".\"}";
					return respond(fd, &response);
				}
				DEBUG("\t" << iter->first << ": " << request_object.objectValues[iter->first]->stringify(true))
			}

			if(this->routemap[route_key]->requires_human){
				if(!request_object.HasObj("captcha", STRING)){
					if(request_type == HTTP_FORM){
						view.parameters["status"] = "An answer is required for the captcha.";
						return respond_http(fd, &view, &response);
					}
					response.body = "{\"error\":\"An answer is required for the captcha.\"}";
					return respond(fd, &response);
				}else{
					if(request_object.GetStr("captcha") != session->captcha){
						session->captcha.clear();
						if(request_type == HTTP_FORM){
							view.parameters["status"] = "Provided answer to the captcha is incorrect.";
							return respond_http(fd, &view, &response);
						}
						response.body = "{\"error\":\"Provided answer to the captcha is incorrect.\"}";
						return respond(fd, &response);
					}
					session->captcha.clear();
				}
			}
		}


		if(request_type == HTTP_FORM){
			if(this->routemap[route_key]->form_function == nullptr){
				response.body = "{\"error\":\"Bad form route.\"}";
				return respond(fd, &response);
			}

			view = this->routemap[route_key]->form_function(&request_object);
			request_type = HTTP;
		}


		if(request_type == HTTP_API){
			if(this->routemap[route_key]->function != nullptr){
				response.body = this->routemap[route_key]->function(&request_object);
				return respond(fd, &response);
			}else if(this->routemap[route_key]->token_function != nullptr){
				if(!request_object.HasObj("token", STRING)){
					response.body = "{\"error\":\"'token' requires a string.\"}";
					return respond(fd, &response);
				}else{
					JsonObject* token = new JsonObject();
					try{
						token->parse(this->encryptor->decrypt(JsonObject::deescape(request_object.GetStr("token"))).c_str());
						response.body = this->routemap[route_key]->token_function(&request_object, token);
						return respond(fd, &response);
					}catch(const std::exception& e){
						DEBUG(e.what())
						response.body = INSUFFICIENT_ACCESS;
						return respond(fd, &response);
					}
				}
			}
			request_type = HTTP;
		}

		if(request_type == HTTP){
			return respond_http(fd, &view, &response);
		}
		
		return this->respond_404(fd);
	};

	// Now run forever as many threads as possible.
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
