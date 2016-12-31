#pragma once

#include <stdexcept>
#include <iostream>

template <class T>
class QueueNode{
public:
	QueueNode(T new_value):value(new_value), prev(0), next(0){}
	T value;
	QueueNode<T>* prev;
	QueueNode<T>* next;
};

template <class T>
class Queue{
private:
	QueueNode<T>* head;
	QueueNode<T>* tail;
public:
	int size;

	Queue():head(0), tail(0), size(0){}

	void queue(T new_value){
		QueueNode<T>* new_node = new QueueNode<T>(new_value);
		new_node->next = this->head;
		this->head = new_node;
		if(new_node->next != 0){
			new_node->next->prev = this->head;
		}
		if(this->tail == 0){
			this->tail = new_node;
		}
		this->size++;
	}

	T dequeue(){
		if(this->tail == 0){
			throw std::runtime_error("Queue::deque");
		}
		QueueNode<T>* result_node = this->tail;
		if(this->tail->prev == 0){
			this->head = 0;
			this->tail = 0;
		}else{
			this->tail->prev->next = 0;
			this->tail = this->tail->prev;
		}
		T result = result_node->value;
		delete result_node;
		this->size--;
		return result;
	}

	friend std::ostream& operator<<(std::ostream& os, const Queue& queue){
		os << '(' << queue.size << ") ";
		QueueNode<T>* next_node = queue.head;
		while(next_node != 0){
			os << next_node->value << ", ";
			next_node = next_node->next;
		}
		return os;
	}
};
