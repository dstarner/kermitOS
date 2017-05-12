#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <linkedlist.h>


struct linkedlist * ll_create() {
  struct linkedlist * list = kmalloc(sizeof(struct linkedlist));
  if (list == NULL) {
    return NULL;
  }
  
  list->size = 0;
  list->head = NULL;
  return list;
}


int ll_add(struct linkedlist * list, void * data, unsigned *index_ret) {
  (void) index_ret;
  // kprintf("ADD, List size: %d, ", ll_num(list));
  // kprintf("%p\n", (void *) &(list));

  if (list->head == NULL) {
    list->head = kmalloc(sizeof(struct linkedlist_node));
    if (list->head == NULL) {return 1;}
    list->head->data = data;
    list->head->next = NULL;
    list->size = 1;
    // kprintf("NEW HEAD DONE\n\n");
    return 0;
  }

  struct linkedlist_node * current = list->head;

  int i = 1;
  while (current->next != NULL) {
    KASSERT(current);
    // kprintf("ITERATING TO %d\n", i);
    current = current->next;
    // kprintf("%p\n", (void *) &(current->next->data));
    i++;
  }


  current->next = kmalloc(sizeof(struct linkedlist_node));
  if (current->next == NULL) {return 1;}
  current->next->data = data;
  current->next->next = NULL;
  list->size++;
  // kprintf("DONE\n\n");
  return 0;
}

void * ll_get(struct linkedlist * list, unsigned int index) {
  if (list->size < index) {
    return NULL;
  }
  unsigned int count = 0;
  struct linkedlist_node * current = list->head;

  while (current != NULL) {
    if (count == index) {
      return current->data;
    }
    count++;
    current = current->next;
  }
  return NULL;
}

unsigned int ll_num(struct linkedlist * list) {
  return list->size;
}

void ll_destroy(struct linkedlist * list) {
  struct linkedlist_node * current = list->head;
  struct linkedlist_node * next;

  while (current != NULL) {
    next = current->next;
    kfree(current);
    current = next;
  }

  kfree(list);
}


void ll_setsize(struct linkedlist * list, unsigned int size) {
  list->size = size;
}

void ll_remove(struct linkedlist * list, unsigned int index) {
  if (list->size < index) return;

  struct linkedlist_node * current = list->head;

  // If removing head
  if (index == 0) {
    list->head = current->next;
    kfree(current);
    return;
  }

  // Get the previous node
  for (unsigned int i=0; current !=NULL && i < index-1; i++)
    current = current->next;

  if (current == NULL || current->next == NULL)
    return;

  // Node temp->next is the node to be deleted
  // Store pointer to the next of node to be deleted
  struct linkedlist_node *next = current->next->next;

  // Unlink the node from linked list
  kfree(current->next);  // Free memory

  current->next = next;  // Unlink the deleted node from list

}
