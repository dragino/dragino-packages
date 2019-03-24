/*
 * generic_list.h
 *
 */

#ifndef GENERIC_LIST_H_
#define GENERIC_LIST_H_

#include <stdbool.h>

typedef struct Node{
	void* data;
	struct Node* next;
}list_node, *pnode;

typedef struct List{
	struct Node* head;
	struct Node* tail;
	long  length;
}linked_list, *plist;

/*intialize the linked list*/
void list_init(plist list);
/*destroy the linked list*/
void list_destroy(plist list, void(*destroy)(void*));
/*insert the node at the tail of the list*/
void list_insert_at_tail(plist list, void* data, int size, void(*assign)(void*,const void*));
/*delete the node at the head of the list*/
void list_delete_at_head(plist list, void(*destroy)(void*));
/*serchar node*/
bool list_search(plist list, void* key, void* data, int(*compare)(const void*, const void*), void(*deep_copy)(void*, const void*));
/*search node and delete it from the list */
bool list_search_and_delete(plist list, void* key, void* data, int(*compare)(const void*, const void*), void(*deep_copy)(void*, const void*), void(*destroy)(void*));
/*search node and update it */
void list_search_and_update(plist list, void* key, void* new_data, int size, int(*compare)(const void*, const void*), void(*assign)(void*, const void*));
/*judge where the list is empty*/
bool list_is_empty(plist list);

#endif /* GENERIC_LIST_H_ */
