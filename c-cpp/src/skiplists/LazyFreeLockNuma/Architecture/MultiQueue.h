#ifndef MULTI_QUEUE_H_
#define MULTI_QUEUE_H_

#include "Nodes.h"
#include <stdlib.h>

typedef struct multi_node {
  node_t* target;
  struct multi_node* next;
} multi_node_t;

multi_node_t* constructMultiNode(node_t* target);

typedef struct multi_queue {
  multi_node_t* head;
  multi_node_t* tail;
  multi_node_t* sentinel;
} multi_queue_t;

multi_queue_t* constructMultiQueue();
void destructMultiQueue(multi_queue_t* jobs);
void multi_push(multi_queue_t* jobs, node_t* target);
multi_node_t* multi_pop(multi_queue_t* jobs);

#endif
