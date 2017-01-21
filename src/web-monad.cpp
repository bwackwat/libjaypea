#include "web-monad.hpp"

WebMonad::WebMonad(std::string hostname, std::string new_public_directory, std::string ssl_certificate, std::string ssl_private_key, uint16_t http_port, uint16_t https_port)
:public_directory(new_public_directory),
redirecter(new EpollServer(http_port, 10)),
server(new PrivateEventServer(ssl_certificate, ssl_private_key, https_port, 10)){
	std::string response_body = "<html>\n"
		"<head>\n"
		"<title>301 Moved Permanently</title>\n"
		"</head>\n"
		"<body>\n"
		"<h1>301 Moved Permanently</h1>\n"
		"<p>This page has permanently moved to <a href=\"https://" +
		hostname +
		"/\">https://" +
		hostname +
		"/</a>.</p>\n"
		"</body>\n"
		"</html>";
	std::string response = "HTTP/1.1 301 Moved Permanently\n" 
		"Location: https://" + hostname + "/\n"
		"Accept-Ranges: bytes\n"
		"Content-Type: text/html\n"
		"Content-Length: " + std::to_string(response_body.length()) + "\n"
		"\n\n" +
		response_body;
	this->http_response = response.c_str();
	this->http_response_length = response.length();
}

void WebMonad::route(std::string method, std::string path, std::function<std::string(JsonObject*)> function, std::unordered_map<std::string, JsonType> requires){
	if(path[path.length() - 1] != '/'){
		path += '/';
	}
	this->routemap[method + " /api" + path] = new Route(function, requires);
}

void WebMonad::start(){
	this->redirecter->on_read = [&](int fd, const char*, ssize_t)->ssize_t{
		this->redirecter->send(fd, this->http_response, this->http_response_length);
		return -1;
	};
	this->redirecter->run(true);

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

	this->server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT(data)
		if(this->server->send(fd, response_header.c_str(), response_header.length())){
			return -1;
		}
		PRINT("DELI:" << response_header)
		
		std::string response_length;
		std::string response_body = std::string();

		JsonObject r_obj(OBJECT);

		enum RequestResult r_type = this->parse_request(data, &r_obj);

		std::string api_route = r_obj.objectValues["route"]->stringValue;

		if(r_type == API || r_type == OTHER){
			// JSON API request with or without HTTP.
			if(r_obj.objectValues["route"]->stringValue.length() >= 4 &&
			!Util::strict_compare_inequal(r_obj.objectValues["route"]->stringValue.c_str(), "/api", 4)){
				api_route = r_obj.objectValues["method"]->stringValue + ' ' + api_route;
				if(api_route.length() > 0 && api_route[api_route.length() - 1] != '/'){
					api_route += '/';
				}
			}
			PRINT("APIR:" << api_route)

			if(this->routemap.count(api_route)){
				bool good_call = true;
				for(auto iter = this->routemap[api_route]->requires.begin(); iter != this->routemap[api_route]->requires.end(); ++iter){
					if(!r_obj.objectValues.count(iter->first) ||
					r_obj.objectValues[iter->first]->type != iter->second){
						// Missing parameter
						response_body = "{\"error\":\"'" + iter->first + "' requries a " + JsonObject::typeString[iter->second] + ".\"}";
						good_call = false;
						break;
					}
				}
				if(good_call){
					if((response_body = this->routemap[api_route]->function(&r_obj)).empty()){
						response_body = "{\"error\":\"The data could not be acquired.\"}";
					}
				}
			}else{
				response_body = "{\"error\":\"Invalid API route.\"}";
			}
		}else{
			// "Regular" HTTP request.
			std::string clean_route = this->public_directory;
			for(size_t i = 0; i < r_obj.objectValues["route"]->stringValue.length(); ++i){
				if(i < r_obj.objectValues["route"]->stringValue.length() - 1 &&
				r_obj.objectValues["route"]->stringValue[i] == '.' &&
				r_obj.objectValues["route"]->stringValue[i + 1] == '.'){
					continue;
				}
				clean_route += r_obj.objectValues["route"]->stringValue[i];
			}

			ssize_t len;
			struct stat route_stat;
			char buffer[BUFFER_LIMIT];

			if(lstat(clean_route.c_str(), &route_stat) < 0){
				perror("lstat");
				ERROR("lstat " << clean_route)
				response_body = HTTP_404;
			}else if(S_ISDIR(route_stat.st_mode)){
				clean_route += "/index.html";
				if(lstat(clean_route.c_str(), &route_stat) < 0){
					perror("lstat index.html");
					ERROR("lstat index.html " << clean_route)
					response_body = HTTP_404;
				}
			}
			PRINT(route_stat.st_mode)
			if(response_body.empty() && S_ISREG(route_stat.st_mode)){
				PRINT("SEND FILE:" << clean_route)
				response_length = "Content-Length: " + std::to_string(route_stat.st_size) + "\n\n";
				if(this->server->send(fd, response_length.c_str(), response_length.length())){
					return -1;
				}
				PRINT("DELI:" << response_length)

				int file_fd;
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
		}
		if(!response_body.empty()){
			response_length = "Content-Length: " + std::to_string(response_body.length()) + "\n\n";
			if(this->server->send(fd, response_length.c_str(), response_length.length())){
				return -1;
			}
			PRINT("DELI:" << response_length)
			if(this->server->send(fd, response_body.c_str(), response_body.length())){
				return -1;
			}
			PRINT("DELI:" << response_body)
		}

		PRINT("JSON:" << r_obj.stringify(true))
		return data_length;
	};

	// Should not return.
	this->server->run();

	ERROR("something super broke")
}

enum RequestResult WebMonad::parse_request(const char* request, JsonObject* request_obj){
	std::string new_key = "";
	std::string new_value = "";
	int state = 0;
	const char* it;
	enum RequestResult result = OTHER;
	bool exit_http_parse = false;

	request_obj->objectValues["method"] = new JsonObject(STRING);
	request_obj->objectValues["route"] = new JsonObject(STRING);
	request_obj->objectValues["protocol"] = new JsonObject(STRING);

	/*
		0 = get method
		1 = get route
		2 = get route parameter key
		3 = get route parameter value
		4 = get protcol
		5 = get header key
		6 = get header value
		7+ = finding newlines and carriage returns
		8 = found \r\n\r\n, if api route, will parse json in request body
	*/

	for(it = request; *it; ++it){
		if(exit_http_parse){
			break;
		}
		switch(*it){
		case ' ':
			if(state == 0){
				if(new_value.length() == 0){
					exit_http_parse = true;
				}else{
					request_obj->objectValues["method"]->type = STRING;
					request_obj->objectValues["method"]->stringValue = new_value;
					state = 1;
					new_value = "";
				}
				continue;
			}else if(state == 1){
				if(new_value.length() == 0){
					exit_http_parse = true;
				}else{
					request_obj->objectValues["route"]->type = STRING;
					request_obj->objectValues["route"]->stringValue = new_value;
					state = 4;
					new_value = "";
				}
				continue;
			}else if(state == 2){
				state = 4;
				continue;
			}else if(state == 3){
				if(!request_obj->objectValues.count(new_key)){
					request_obj->objectValues[new_key] = new JsonObject();
				}
				request_obj->objectValues[new_key]->type = STRING;
				request_obj->objectValues[new_key]->stringValue = new_value;
				state = 4;
				new_value = "";
				continue;
			}
			break;
		case '?':
			if(state == 1){
				request_obj->objectValues["route"]->type = STRING;
				request_obj->objectValues["route"]->stringValue = new_value;
				new_key = "";
				state = 2;
				continue;
			}
			break;
		case '=':
			if(state == 2){
				new_value = "";
				state = 3;
				continue;
			}
			break;
		case '&':
			if(state == 3){
				if(!request_obj->objectValues.count(new_key)){
					request_obj->objectValues[new_key] = new JsonObject();
				}
				request_obj->objectValues[new_key]->type = STRING;
				request_obj->objectValues[new_key]->stringValue = new_value;
				new_key = "";
				state = 2;
				continue;
			}
			break;
		case '\n':
			if(state == 4){
				request_obj->objectValues["protocol"]->type = STRING;
				request_obj->objectValues["protocol"]->stringValue = new_value;
				new_key = "";
				state = 5;
				continue;
			}else if(state == 5){
				state = 7;
			}else if(state == 6){
				if(!request_obj->objectValues.count(new_key)){
					request_obj->objectValues[new_key] = new JsonObject();
				}
				request_obj->objectValues[new_key]->type = STRING;
				request_obj->objectValues[new_key]->stringValue = new_value;
				new_key = "";
				state = 5;
				continue;
			}
			break;
		case ':':
			if(state == 5){
				++it;
				new_value = "";
				state = 6;
				continue;
			}
			break;
		}
		switch(state){
		case 0:
		case 1:
		case 3:
		case 4:
		case 6:
			new_value += *it;
			break;
		case 2:
		case 5:
			new_key += *it;
			break;
		default:
			if(*it == '\r' || *it == '\n'){
				state++;
			}
			if(state >= 8){
				exit_http_parse = true;
				result = HTTP;
				std::cout << "HTTP ";
			}
		}
	}

	if(request_obj->objectValues["route"]->stringValue.length() >= 4 &&
	request_obj->objectValues["route"]->stringValue.substr(0, 4) == "/api"){
		request_obj->parse(it);
		result = API;
		std::cout << "JSON ";
	}

	return result;
}
