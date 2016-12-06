#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <thread>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "util.hpp"

/*
 * Returns true if the c-strings are strictly inequal.
 */
bool strict_compare_inequal(const char* first, const char* second, int count){
	if(count){
		for(int i = 0; i < count; ++i){
			if(first[i] != second[i]){
				return true;
			}
		}
	}else{
		for(int i = 0;; ++i){
			if(first[i] != second[i] ||
			first[i] + second[i] == first[i] || 
			first[i] + second[i] == second[i]){
				return true;
			}
		}
	}
	return false;
}
