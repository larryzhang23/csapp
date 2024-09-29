/* 
 * Code for basic C skills diagnostic.
 * Developed for courses 15-213/18-213/15-513 by R. E. Bryant, 2017
 */

/*
 * This program implements a queue supporting both FIFO and LIFO
 * operations.
 *
 * It uses a singly-linked list to represent the set of queue elements
 */

#include <stdlib.h>
#include <stdio.h>

#include "harness.h"
#include "queue.h"

/*
  Create empty queue.
  Return NULL if could not allocate space.
*/
queue_t *q_new()
{
    queue_t *q =  malloc(sizeof(queue_t));
    /* What if malloc returned NULL? */
    if (q != NULL) {
      q->head = NULL;
      q->tail = NULL;
      q->node_count = 0;
    }
    return q;
}

/* Free all storage used by queue */
void q_free(queue_t *q)
{   
    list_ele_t *ptr;
    if (q == NULL) 
      return;
    /* How about freeing the list elements? */
    while (q->head != NULL) {
      ptr = q->head;
      q->head = q->head->next;
      free(ptr);
    }
    /* Free queue structure */
    free(q);
}

/*
  Attempt to insert element at head of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
 */
bool q_insert_head(queue_t *q, int v)
{
    list_ele_t *newh;
    /* What should you do if the q is NULL? */
    if (q == NULL) 
      return false;
    newh = malloc(sizeof(list_ele_t));
    /* What if malloc returned NULL? */
    if (newh == NULL)
      return false;
    newh->value = v;
    newh->next = q->head;
    q->head = newh;
    if (q->node_count == 0) {
      q->tail = newh;
    }
    /* increment the node count */
    q->node_count++;
    return true;
}


/*
  Attempt to insert element at tail of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
 */
bool q_insert_tail(queue_t *q, int v)
{
    /* You need to write the complete code for this function */
    /* Remember: It should operate in O(1) time */
    list_ele_t *newt;
    if (q == NULL)
      return false;
    newt = malloc(sizeof(list_ele_t));
    if (newt == NULL)
      return false;
    newt->value = v;
    newt->next = NULL;
    if (q->tail == NULL) {
      q->tail = newt;
      q->head = newt;
    } else {
      q->tail->next = newt;
      q->tail = newt;
    }
    q->node_count++;
    return true;
}

/*
  Attempt to remove element from head of queue.
  Return true if successful.
  Return false if queue is NULL or empty.
  If vp non-NULL and element removed, store removed value at *vp.
  Any unused storage should be freed
*/
bool q_remove_head(queue_t *q, int *vp)
{
    /* You need to fix up this code. */
    list_ele_t *temp;
    if (q == NULL || q->head == NULL)
      return false;
    temp = q->head;
    q->head = q->head->next;
    q->node_count--;
    if (q->node_count == 0)
      q->tail = NULL;
    if (vp != NULL) 
      *vp = temp->value;
    free(temp);
    return true;
}

/*
  Return number of elements in queue.
  Return 0 if q is NULL or empty
 */
int q_size(queue_t *q)
{
    /* You need to write the code for this function */
    /* Remember: It should operate in O(1) time */
    if (q == NULL) 
      return 0;
    return q->node_count;
}

/*
  Reverse elements in queue.

  Your implementation must not allocate or free any elements (e.g., by
  calling q_insert_head or q_remove_head).  Instead, it should modify
  the pointers in the existing data structure.
 */
void q_reverse(queue_t *q)
{
    /* You need to write the code for this function */
    list_ele_t *fp1, *fp2, *temp;
    if (q == NULL || q->node_count <= 1)
      return;
    q->tail = q->head;
    fp1 = NULL;
    fp2 = q->head;
    while (fp2 != NULL) {
      temp = fp2->next;
      fp2->next = fp1;
      fp1 = fp2;
      fp2 = temp;
    }
    q->head= fp1;
}

