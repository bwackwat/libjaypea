#include <iostream>

#include "queue.hpp"
#include "util.hpp"

int main(){
	Queue<int> one;
	PRINT(one)

	one.queue(11);
	one.queue(12);
	one.queue(13);

	PRINT(one)
	one.queue(100);
	PRINT(one.dequeue())
	PRINT(one)
	PRINT(one.dequeue())
	PRINT(one)
	PRINT(one.dequeue())
	PRINT(one)
	PRINT(one.dequeue())
	PRINT(one)

	one.queue(10000);
	PRINT(one)
	PRINT(one.dequeue())
	PRINT(one)
}
