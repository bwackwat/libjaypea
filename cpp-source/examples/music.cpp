#include <algorithm>
#include <iterator>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "http-api.hpp"

struct Songset{
	std::string name;
	std::unordered_map<std::string /* directory path */, struct Songset*> sets;
	std::vector<std::string> songs;
};

static struct Songset* get_songset(std::string path){
	DIR *dir;
	struct dirent *ent;
	struct Songset* new_set = new struct Songset;

	if((dir = opendir(path.c_str())) <= 0){
		perror("opendir");
		throw std::runtime_error("opendir");
	}

	while(true){
		if((ent = readdir(dir)) < 0){
			perror("readdir");
			throw std::runtime_error("readdir");
		}else if(ent == 0){
			break;
		}else if(ent->d_name[0] == '.'){
			PRINT("SKIP: " << ent->d_name)
			continue;
		}
		
		struct stat route_stat;
		std::string new_path = path + '/' + ent->d_name;
		if(lstat(new_path.c_str(), &route_stat) < 0){
			perror("lstat");
			throw std::runtime_error("lstat");
		}
		
		if(S_ISDIR(route_stat.st_mode)){
			PRINT("SET: " + new_path)
			new_set->sets[new_path] = get_songset(new_path);
			new_set->sets[new_path]->name = ent->d_name;
		}else if(S_ISREG(route_stat.st_mode)){
			PRINT("SONG: " + new_path)
			new_set->songs.push_back(new_path);
		}else{
			PRINT("SKIP: " + new_path)
		}
	}
	
	if(closedir(dir) < 0){
		perror("closedir");
		throw std::runtime_error("closedir");
	}
	
	return new_set;
}

static struct Songset* find_set(struct Songset* songset, std::string set){
	struct Songset* result = 0;
	for(auto iter = songset->sets.begin(); iter != songset->sets.end(); ++iter){
		if(iter->second->name == set){
			return iter->second;
		}
		if((result = find_set(iter->second, set)) != 0){
			return result;
		}
	}
	return result;
}

int main(int argc, char **argv){
	std::string public_directory;
	std::string ssl_certificate;
	std::string ssl_private_key;
	int port = 443;
	bool http = false;
	int cache_megabytes = 30;
	std::string password = "";

	Util::define_argument("public_directory", public_directory, {"-pd"});
	Util::define_argument("ssl_certificate", ssl_certificate, {"-crt"});
	Util::define_argument("ssl_private_key", ssl_private_key, {"-key"});
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("http", &http);
	Util::define_argument("cache_megabytes", &cache_megabytes, {"-cm"});
	Util::define_argument("password", password, {"-pw"});
	Util::parse_arguments(argc, argv, "This serves music over HTTPS, through an API.");

	EpollServer* server;
	if(http){
		server = new EpollServer(static_cast<uint16_t>(port), 10);
	}else{
		server = new TlsEpollServer(ssl_certificate, ssl_private_key, static_cast<uint16_t>(port), 10);
	}
	
	HttpApi api(public_directory, server);
	api.set_file_cache_size(cache_megabytes);
	
	struct Songset* library = get_songset("../../Music");
	
	api.route("GET", "/set", [&](JsonObject* json)->std::string{
		struct Songset* search = find_set(library, json->GetStr("set"));
		if(search == 0){
			return "{\"error\",\"No set.\"}";
		}
		std::stringstream ss;
		for(std::string song : search->songs){
			if(song == search->songs.back()){
				ss << '\"' << song << '\"';
			}else{
				ss << '\"' << song << "\",";
			}
		}
		return "{\"songs\":[" + ss.str() + "]}";
	}, {{"token", STRING}, {"set", STRING}});

	api.start();
}
