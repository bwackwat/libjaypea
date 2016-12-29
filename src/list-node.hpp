#pragma once

#include <stdexcept>
#include <iostream>

template <class T>
class Node{
public:
	T value;
	Node<T>* next;

	Node(T new_value):value(new_value){}
};

template <class T>
class NodeList{
public:
	ListNode():size(0), head(0){}

	void add(T new_value){
		if(size <= 0){
			this->value = new_value;
			this->size++;
		}else{
			Node<T>* new_node = new Node(new_value);
		}
	}

private:
	int size;
	Node<T>* head;
};
