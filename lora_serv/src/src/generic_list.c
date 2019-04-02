#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "generic_list.h"

void list_init(plist list) {
	list->head = NULL;
	list->tail = NULL;
	list->length = 0;
}

void list_destroy(plist list, void(*destroy)(void*)) {
	pnode node;
	pnode temp;
	node = list->head;
	while (node != NULL) {
		temp = node->next;
		if(destroy != NULL){
			destroy(node->data);
		}
		free(node->data);
		free(node);
		node = temp;
	}
}

bool list_is_empty(plist list) {
	if(list->head == NULL)
		return true;
	else
		return false;
}

void list_insert_at_tail(plist list, void* data, int size, void(*assign)(void*, const void*)) {
	pnode node;
	node = malloc(sizeof(list_node));
	node->next = NULL;
	node->data = malloc(size);
	if(assign != NULL) {
		assign(node->data, data);
	}
	if(list->head == NULL) {
		list->head = node;
		list->tail = node;
	} else {
		list->tail->next = node;
		list->tail = node;
	}
	list->length++;
}

void list_delete_at_head(plist list, void(*destroy)(void*)) {
	pnode node;
	if(list->head == NULL){
		return;
	}
	node = list->head->next;
	if(destroy != NULL){
		destroy(list->head->data);
	}
	free(list->head->data);
	free(list->head);
	if(node == NULL){
		list->head = NULL;
		list->tail = NULL;
	} else {
		list->head = node;
	}
	list->length--;
}

bool list_search(plist list, void* key, void* data, int(*compare)(const void*, const void*), void(*deep_copy)(void*,const void*)) {
	pnode node;
	node = list->head;
	while (node != NULL) {
		if(compare(node->data, key) == 0){
			deep_copy(data, node->data);
			return true;
		}
		node = node->next;
	}
	return false;
}

bool list_search_and_delete(plist list, void* key, void* data, int(*compare)(const void*, const void*), void(*deep_copy)(void*, const void*), void(*destroy)(void*)) {
	pnode node;
	pnode front_node;
	front_node = list->head;
	node = list->head;
	if (node == NULL)
		return false;
	if (compare(node->data, key) == 0) {
		deep_copy(data, node->data);
		list_delete_at_head(list, destroy);
		list->length--;
		return true;
	}
	node = node->next;
	while (node != NULL){
		if (compare(node->data, key) == 0) {
			front_node->next = node->next;
			/*if the node is the tail*/
			if (node->next == NULL) {
				list->tail = front_node;
			}
			deep_copy(data, node->data);
			if (destroy != NULL){
				destroy(node->data);
			}
			free(node->data);
			free(node);
			list->length--;
			return true;
		}
		front_node = front_node->next;
		node = node->next;
	}
	return false;
}

void list_search_and_update (plist list, void* key, void* new_data, int size, int(*compare)(const void*, const void*),void(*assign)(void*, const void*)) {
	pnode node;
	node = list->head;
	while (node != NULL) {
		if (compare(node->data, key) == 0) {
		   assign(node->data, new_data);
		   break;
		}
		node = node->next;
	}
	if (node == NULL) {
		list_insert_at_tail(list, new_data, size, assign);
	}
}
