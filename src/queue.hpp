#pragma once

#include <stdexcept>
#include <iostream>

template <class T>
class Queue{
private:
	QueueNode<T>* head;
	QueueNode<T>* tail;
public:
	readonly int size;

	Queue():size(0),head(0),tail(0){}

	void push(T new_value){
		StackNode<T>* new_node = new StackNode(new_value);
		new_node->next = this->head;
		this->head = new_node;
		if(this->tail == 0){
			this->tail = new_node;
		}
		this->size++;

	T pop(){
		if(this->end == 0){
			throw std::runtime_error("Stack::pop");
		}
		QueueNode<T>* result_node = this->end;
		this->end = this->end->prev;
		this->end->next = 0;
		T result = result_node->value;
		delete result_node;
		this->size--;
		return result;
	}

	friend std::ostream& operator<<(std::ostream& os, const Stack& stack){
		os << '(' << node.value << ") ";
		StackNode* next_node = stack.head;
		while(next_node != 0){
			os << next_node->value << ", ";
			next_node = next_node->next;
		}
		return os;
	}
}


template <class T>
class QueueNode{
public:
	QueueNode(T new_value):value(new_value), next(0){}
	T value;
	QueueNode* prev;
	QueueNode* next;
};
