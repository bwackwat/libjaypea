EpollServer http_server(static_cast<uint16_t>(http_port), 10);
http_server.on_read = [&](int fd, const char* data, ssize_t)->ssize_t{
	const char* it = data;
	int state = 1;
	std::string url = "";
	url += hostname;

	for(; *it; ++it){
		if(state == 2){
			if(*it == '\r' || *it == '\n' || *it == ' '){
				break;
			}else{
				url += *it;
			}
		}else if(*it == ' '){
			state = 2;
		}
	}

	std::string response = Util::get_redirection_html(url);
	const char* http_response = response.c_str();
	size_t http_response_length = response.length();

	http_server.send(fd, http_response, http_response_length);
	return -1;
};
http_server.run(true, 1);
