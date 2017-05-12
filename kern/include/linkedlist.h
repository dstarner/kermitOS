#ifndef _LINKEDLIST_H_
#define _LINKEDLIST_H_

#include <types.h>

struct linkedlist_node {
  void * data; // Pointer to the data (must be cast after).
  struct linkedlist_node *next; // Pointing to next in list
};

struct linkedlist {

  // Head of list
  struct linkedlist_node *head;

  // Size of list
  unsigned int size;

};

struct linkedlist * ll_create(void);

unsigned int ll_num(struct linkedlist *);

void * ll_get(struct linkedlist * list, unsigned int index);

int ll_add(struct linkedlist * list, void * data, unsigned *index_ret);

void ll_destroy(struct linkedlist *);

void ll_clear_nodes(struct linkedlist *);

void ll_remove(struct linkedlist * list, unsigned int index);

void ll_setsize(struct linkedlist *, unsigned int);

#endif
