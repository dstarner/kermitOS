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

/* Adds to a linked list */
void add_to_llist(struct linkedlist *, void *);

/* Gets index from a linked list */
void * get_from_llist(struct linkedlist *, unsigned int);

/* Deep copy a linked list */
struct linkedlist * deep_copy_llist(struct linkedlist *);
