#pragma once

#include <stdexcept>
#include <iostream>
#include <atomic>

template <class T>
class StackNode{
public:
	StackNode(T new_value):value(new_value), next(0){}
	T value;
	StackNode<T>* next;
};

template <class T>
class Stack{
private:
	StackNode<T>* head;
public:
	std::atomic<int> size;

	Stack():head(0), size(0){}

	void push(T new_value){
		StackNode<T>* new_node = new StackNode<T>(new_value);
		new_node->next = this->head;
		this->head = new_node;
		this->size++;
	}

	T pop(){
		if(this->head == 0){
			throw std::runtime_error("Stack::pop");
		}
		StackNode<T>* result_node = this->head;
		this->head = this->head->next;
		T result = result_node->value;
		delete result_node;
		this->size--;
		return result;
	}

	friend std::ostream& operator<<(std::ostream& os, const Stack& stack){
		os << '(' << stack.size << ") ";
		StackNode<T>* next_node = stack.head;
		while(next_node != 0){
			os << next_node->value << ", ";
			next_node = next_node->next;
		}
		return os;
	}
};
