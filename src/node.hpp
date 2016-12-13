#pragma once

#include <stdexcept>
#include <iostream>

template <class T>
class Node{
public:
	Node(T new_value):value(new_value), next(0){}
	Node():value(0), next(0){}

	void push(T new_value){
		Node<T>* new_node = new Node(new_value);
		new_node->next = this->next;
		this->next = new_node;
		this->value++;
	}

	T pop(){
		if(this->next == 0){
			throw std::runtime_error("Node::pop");
		}
		Node<T>* result_node = this->next;
		this->next = this->next->next;
		T result = result_node->value;
		delete result_node;
		this->value--;
		return result;
	}

	friend std::ostream& operator<<(std::ostream& os, const Node& node){
		os << '(' << node.value << ") ";
		Node* next_node = node.next;
		while(next_node != 0){
			os << next_node->value << ", ";
			next_node = next_node->next;
		}
		return os;
	}

	T value;
private:
	Node<T>* next;
};
