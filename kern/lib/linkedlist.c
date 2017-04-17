#include <linkedlist.h>


/* Adds to a linked list */
void add_to_llist(struct linkedlist list *, void * datum) {

  struct linkedlist_node * node = kmalloc(sizeof(struct linkedlist_node));

  // Set the data
  node->data = datum;
  node->next = NULL;

  // Increment the size
  list->size++;

  // If the list is NULL, then set to the head of the list.
  if (list->head == NULL) {
    list->head = node;
    return;
  }

  // Get the head of the list
  struct linkedlist_node * current = list->head;

  while (current->next != NULL) {
    current = current->next;
  }

  current->next = node;

}

/* Gets index from a linked list */
void * get_from_llist(struct linkedlist * list, unsigned int index) {

  // If the list is smaller than requested size or null list
  if (list == NULL || list->size < index || list->head == NULL) return NULL;

  struct linkedlist_node * current = list->head;
  unsigned int count;

  while (current != NULL) {

    // If we found what we want
    if (count == index) return(current->data);

    count++;
    current = current->next;
  }

  // Not found
  return NULL;

}

//quu..__
// $$$b  `---.__
//  "$$b        `--.                          ___.---uuudP
//   `$$b           `.__.------.__     __.---'      $$$$"              .
//     "$b          -'            `-.-'            $$$"              .'|
//       ".                                       d$"             _.'  |
//         `.   /                              ..."             .'     |
//           `./                           ..::-'            _.'       |
//            /                         .:::-'            .-'         .'
//           :                          ::''\          _.'            |
//          .' .-.             .-.           `.      .'               |
//          : /'$$|           .@"$\           `.   .'              _.-'
//         .'|$u$$|          |$$,$$|           |  <            _.-'
//         | `:$$:'          :$$$$$:           `.  `.       .-'
//         :                  `"--'             |    `-.     \
//        :##.       ==             .###.       `.      `.    `\
//        |##:                      :###:        |        >     >
//        |#'     `..'`..'          `###'        x:      /     /
//         \                                   xXX|     /    ./
//          \                                xXXX'|    /   ./
//          /`-.                                  `.  /   /
//         :    `-  ...........,                   | /  .'
//         |         ``:::::::'       .            |<    `.
//         |             ```          |           x| \ `.:``.
//         |                         .'    /'   xXX|  `:`M`M':.
//         |    |                    ;    /:' xXXX'|  -'MMMMM:'
//         `.  .'                   :    /:'       |-'MMMM.-'
//          |  |                   .'   /'        .'MMM.-'
//          `'`'                   :  ,'          |MMM<
//            |                     `'            |tbap\
//             \                                  :MM.-'
//              \                 |              .''
//               \.               `.            /
//                /     .:::::::.. :           /
//               |     .:::::::::::`.         /
//               |   .:::------------\       /
//              /   .''               >::'  /
//              `',:                 :    .'

void delete_llist(struct linkedlist * list, bool isSegment) {

  // Delete an empty list
  if (list->size > 0 && list->head == NULL) {
    kfree(list);
    return;
  }

  // Start somewhere
  struct linkedlist_node * current = list->head;

  // Iterate over the list
  while (current != NULL) {

    // Delete the actual object
    if (isSegment) {
      struct segment_entry * segment = (struct segment_entry *) current->datum;
      kfree(segment);

    } else {

      struct page_entry * page = (struct page_entry *) current->datum;
      kfree(page);

    }

    // Get the next one and delete the current.
    struct linkedlist_node * next = current->next;
    kfree(current);

    // Set new current
    current = next;
  }

  // Free the list
  kfree(list);
}
