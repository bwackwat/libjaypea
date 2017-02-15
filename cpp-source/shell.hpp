#pragma once

#include <atomic>

class Shell{
private:
	int pid;
public:
	std::atomic<bool> opened;
	int input;
	int output;

	Shell();

	bool sopen();
	void sclose();
};
