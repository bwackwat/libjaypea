#include <fstream>

#include "util.hpp"
#include "json.hpp"
#include "distributed-util.hpp"
#include "distribution-client.hpp"

DistributionClient::DistributionClient(std::string new_keyfile, std::string new_password)
:keyfile(new_keyfile),
password(new_password){}

std::string DistributionClient::get(){
	JsonObject distribution(ARRAY);
	std::string response;
	for(auto iter = this->hosts.begin(); iter != this->hosts.end(); ++iter){
		JsonObject* host = new JsonObject(OBJECT);
		host->objectValues["host"] = new JsonObject(iter->first);
		host->objectValues["status"] = new JsonObject("disconnected");
		host->objectValues["services"] = new JsonObject(ARRAY);
		response = std::string();
		if(!iter->second->connected){
			iter->second->communicate(this->password);
		}
		if(iter->second->connected){
			response = iter->second->communicate("get");
		}
		if(!response.empty()){
			host->objectValues["status"]->stringValue = "connected";
			host->objectValues["services"]->parse(response.c_str());
		}
		distribution.arrayValues.push_back(host);
	}
	return distribution.stringify();
}
