#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <linkedlist.h>


struct linkedlist * ll_create() {
  struct linkedlist * list = kmalloc(sizeof(struct linkedlist));
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
