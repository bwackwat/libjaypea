#include <fstream>

#include "util.hpp"
#include "json.hpp"
#include "distributed-util.hpp"
#include "symmetric-tcp-server.hpp"

struct Shell{
	int pid;
	int input;
	int output;
};

static struct Shell* open_shell(){
	struct Shell* new_shell = new struct Shell();
	int shell_pipe[2][2];

	if(pipe(shell_pipe[0]) < 0){
		ERROR("pipe 0")
		delete new_shell;
		return 0;
	}
	
	if(pipe(shell_pipe[1]) < 0){
		ERROR("pipe 1")
		delete new_shell;
		return 0;
	}
	
	if((new_shell->pid = fork()) < 0){
		ERROR("fork")
		delete new_shell;
		return 0;
	}else if(new_shell->pid == 0){
		dup2(shell_pipe[0][0], STDIN_FILENO);
		dup2(shell_pipe[1][1], STDOUT_FILENO);

		close(shell_pipe[0][0]);
		close(shell_pipe[1][1]);
		close(shell_pipe[1][0]);
		close(shell_pipe[0][1]);

		if(execl("/bin/bash", "/bin/bash", static_cast<char*>(0)) < 0){
			ERROR("execl")
		}
		PRINT("execl done!")
		delete new_shell;
		return 0;
	}else{
		close(shell_pipe[0][0]);
		close(shell_pipe[1][1]);

		new_shell->output = shell_pipe[1][0];
		new_shell->input = shell_pipe[0][1];

		return new_shell;
	}
}

static void close_shell(struct Shell* shell){
		kill(shell->pid, SIGTERM);
		close(shell->input);
		close(shell->output);
		delete shell;
		shell = 0;
}

int main(int argc, char** argv){
	int port = 10001;
	std::string keyfile;
	std::string services_path = "artifacts/services.json";
	
	Util::define_argument("port", &port, {"-p"});
	Util::define_argument("keyfile", keyfile, {"-k"});
	Util::define_argument("services_file", services_path, {"-sf"});
	Util::parse_arguments(argc, argv, "This server manages services on its host.");
	
	std::ifstream services_file(services_path);
	JsonObject services;
	if(services_file.is_open()){
		std::string services_data((std::istreambuf_iterator<char>(services_file)), (std::istreambuf_iterator<char>()));
		services.parse(services_data.c_str());
		PRINT("LOADED SERVICES FILE: " << services_path)
		PRINT(services.stringify(true));
	}

	SymmetricEpollServer server(keyfile, static_cast<uint16_t>(port), 1);
	
	struct Shell* shell;
	
	enum ServerState{
		VERIFYING,
		GET_ROUTINE,
		SET_SERVICES,
		IN_SHELL
	} state;
	
	std::string password;
	
	server.on_connect = [&](int){
		state = VERIFYING;
	};

	server.on_read = [&](int fd, const char* data, ssize_t data_length)->ssize_t{
		PRINT("RECV:" << data)
		if(state == VERIFYING){
			if(password.empty()){
				password = std::string(data);
				if(server.send(fd, "Password has been set, please set the configuration.")){
					PRINT("send error 1")
					return -1;
				}
			}else if(Util::strict_compare_inequal(data, password.c_str(), static_cast<int>(password.length()))){
				PRINT("Someone failed to verify their connection to this server!")
				return -1;
			}else if(server.send(fd, "Password accepted.")){
				PRINT("send error 2")
				return -1;
			}
			state = GET_ROUTINE;
			return data_length;
		}else if(state == GET_ROUTINE){
			if(data == GET){
				if(services.HasObj("services", ARRAY)){
					if(server.send(fd, services.stringify(false))){
						PRINT("send error 2")
						return -1;
					}
				}else{
					if(server.send(fd, "No services JSON!")){
						PRINT("send error 2")
						return -1;
					}
				}
			}else if(data == SET){
				if(server.send(fd, "Setting services.")){
					PRINT("send error 3")
					return -1;
				}
				state = SET_SERVICES;
			}else if(data == SHELL){
				if(server.send(fd, "Shell entered.")){
					PRINT("send error 4")
					return -1;
				}
				if((shell = open_shell()) == 0){
					ERROR("shell_routine")
					return -1;
				}
				state = IN_SHELL;
			}else{
				PRINT("unknown command" << data)
				return -1;
			}
		}else if(state == SET_SERVICES){
			services.parse(data);
			if(server.send(fd, services.stringify(true))){
				PRINT("send error 2")
				return -1;
			}
			state = GET_ROUTINE;
		}else if(state == IN_SHELL){
			if(data == EXIT){
				close_shell(shell);
				if(server.send(fd, "Shell is done.")){
					PRINT("send error 2")
					return -1;
				}
				state = GET_ROUTINE;
			}else{
				ssize_t len;
				char shell_data[PACKET_LIMIT];
				if((len = write(shell->input, data, static_cast<size_t>(data_length))) < 0){
					ERROR("write to shell input")
					close_shell(shell);
					return -1;
				}else if(len != data_length){
					ERROR("couldn't write all " << len << ", " << data_length)
				}
				shell_data[0] = '\n';
				shell_data[1] = 0;
				if((len = write(shell->input, shell_data, 1)) < 0){
					ERROR("write n to shell input")
					close_shell(shell);
					return -1;
				}else if(len != 1){
					ERROR("couldn't write n " << len << ", " << data_length)
				}
				sleep(1);
				if((len = read(shell->output, shell_data, PACKET_LIMIT)) < 0){
					ERROR("read from shell output")
					close_shell(shell);
					return -1;
				}
				shell_data[len] = 0;
				PRINT("SHELL:" <<  shell_data)
				if(server.send(fd, shell_data, static_cast<size_t>(len))){
					PRINT("send error 5")
					return -1;
				}
			}
		}else{
			return -1;
		}
		return data_length;
	};
	
	server.run(false, 1);
	
	// This should only return on reboot, necessary for written configuration to activate.
	
	return 0;
}
