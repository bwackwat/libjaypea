#include <iostream>

#include "queue.hpp"
#include "util.hpp"

int main(){
	Queue<int> one;
	PRINT(one)

	one.enqueue(11);
	one.enqueue(12);
	one.enqueue(13);

	PRINT(one)
	one.enqueue(100);
	PRINT(one.dequeue())
	PRINT(one)
	PRINT(one.dequeue())
	PRINT(one)
	PRINT(one.dequeue())
	PRINT(one)
	PRINT(one.dequeue())
	PRINT(one)

	one.enqueue(10000);
	PRINT(one)
	PRINT(one.dequeue())
	PRINT(one)
}
