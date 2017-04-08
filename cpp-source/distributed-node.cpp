#include "util.hpp"
#include "distributed-node.hpp"

DistributedNode::DistributedNode(std::string new_keyfile)
:status(OBJECT),
ddata(OBJECT),
keyfile(new_keyfile){
	uint16_t port = 30000;
	while(true){
		try{
			this->server = new SymmetricEpollServer(this->keyfile, port, 10);	
		}catch(std::runtime_error& e){
			if(errno != 98){
				throw e;
			}
			port++;
			continue;
		}
		break;
	}
	
	this->server->on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT(data)
		JsonObject request;
		request.parse(data);
		PRINT(request.stringify(true))
		JsonObject response(OBJECT);
		
		if(request.HasObj("hash", STRING)){
			PRINT("GIVEN HASH: " << request.GetStr("hash"))
			PRINT("MY HASH: " << this->status.GetStr("hash"))
			if(request.GetStr("hash") == this->status.GetStr("hash")){
				response.objectValues["status"] = new JsonObject("Up to date.");
			}else{
				response.objectValues["status"] = new JsonObject("Out of date.");
			}
		}else{
			response.objectValues["hash"] = new JsonObject(this->status.GetStr("hash"));
		}
		
		if(this->server->send(fd, response.stringify(false))){
			ERROR("DistributedNode send")
			return -1;
		}

		return data_length;
	};
	
	// Returns and runs on a single thread.
	this->server->run(true, 1);
	
	start_thread = std::thread(&DistributedNode::start, this);
	
	PRINT("DISTRIBUTED NODE INITIALIZED.")
}

void DistributedNode::add_client(const char* ip_address, uint16_t port){
	std::string client = std::string(ip_address) + ':' + std::to_string(port);
	this->clients_mutex.lock();
	this->clients[client] = new SymmetricTcpClient(ip_address, port, this->keyfile);
	this->clients_mutex.unlock();
}

bool DistributedNode::set_ddata(std::string data){
	this->ddata.objectValues.clear();
	this->ddata.parse(data.c_str());
	this->status.objectValues["hash"] = new JsonObject(Util::sha256_hash(this->ddata.stringify(false)));
	return true;
}

void DistributedNode::start(){
	std::string response;
	while(true){
		this->clients_mutex.lock();
		for(auto iter = this->clients.begin(); iter != this->clients.end(); ++iter){
			response = iter->second->communicate(status.stringify(false));
			if(response.empty()){
				PRINT(iter->first << " DISCONNECTED")
			}else{
				PRINT(iter->first << " -> " << response)
			}
		}
		this->clients_mutex.unlock();

		sleep(5);
	}
}
