#include <iostream>
#include <cstring>

#include "ponal.hpp"

int main(int argc, char** argv){
	for(int i = 0; i < argc; ++i){
		if(std::strcmp(argv[i], "-h") == 0 ||
		std::strcmp(argv[i], "--help") == 0){
			std::cout  << "See \"ponal -h\" and \"ponald -h\"." << std::endl;
		}
	}

	Ponal ponal(12345);
	ponal.Set("apples", "oranges");
	std::cout << ponal.Get("apples") << std::endl;
}
