#pragma once

#include <atomic>

#include "util.hpp"

class Shell{
private:
	int pid;
public:
	std::atomic<bool> opened;

	int input;
	int output;

	Shell():opened(false){}

	bool sopen(){
		if(this->opened){
			return false;
		}

		int shell_pipe[2][2];

		if(pipe(shell_pipe[0]) < 0){
			ERROR("pipe 0")
			return true;
		}
	
		if(pipe(shell_pipe[1]) < 0){
			ERROR("pipe 1")
			return true;
		}
	
		if((this->pid = fork()) < 0){
			ERROR("fork")
			return true;
		}else if(this->pid == 0){
			dup2(shell_pipe[0][0], STDIN_FILENO);
			dup2(shell_pipe[1][1], STDOUT_FILENO);
			dup2(shell_pipe[1][1], STDERR_FILENO);

			close(shell_pipe[0][0]);
			close(shell_pipe[1][1]);
			close(shell_pipe[1][0]);
			close(shell_pipe[0][1]);

			if(execl("/bin/bash", "/bin/bash", static_cast<char*>(0)) < 0){
				perror("execl");
			}
			PRINT("execl done!")
			return true;
		}else{
			close(shell_pipe[0][0]);
			close(shell_pipe[1][1]);

			this->output = shell_pipe[1][0];
			this->input = shell_pipe[0][1];
		}

		this->opened = true;
		return false;
	}

	void sclose(){
		if(!this->opened){
			return;
		}
		if(kill(this->pid, SIGTERM) < 0){
			ERROR("kill shell")
		}
		if(close(this->input) < 0){
			ERROR("close shell input")
		}
		if(close(this->output) < 0){
			ERROR("close shell output")
		}
		this->opened = false;
	}
};
