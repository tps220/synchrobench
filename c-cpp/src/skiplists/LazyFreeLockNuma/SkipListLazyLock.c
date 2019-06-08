#ifndef SKIPLISTLAZYLOCK_C
#define SKIPLISTLAZYLOCK_C

#include "SkipListLazyLock.h"
#include "Atomic.h"
#include "stdio.h"

extern multi_queue_t** mq;
inline int validateRemoval(node_t* current) {
  return current -> markedToDelete && 
        current -> fresh == 0 && 
        current -> attempts * -1 * numberNumaZones == current -> references;
}

//Inserts a value into the skip list if it doesn't already exist
int add(inode_t *sentinel, int val, node_t* dataLayer, int zone) {
  //store the result of a traversal through the skip list while searching for the target
  inode_t *predecessors[sentinel -> topLevel], *successors[sentinel -> topLevel];
  inode_t *previous = sentinel, *current = NULL;

  //Lazily searches the skip list, disregarding locks
  for (int i = sentinel -> topLevel - 1; i >= 0; i--) {
    current = previous -> next[i];
    while (current -> val < val) {
      previous = current;
      current = previous -> next[i];
    }
    predecessors[i] = previous;
    successors[i] = current;
  }

  //if the value doesn't already exist, create a new index node and return true
  //otherwise return false
  inode_t* candidate = current;
  if (candidate -> val != val) {
    const int topLevel = getRandomLevel(sentinel -> topLevel);
    inode_t* insertion = constructIndexNode(val, topLevel, dataLayer, zone);
    for (int i = 0; i < topLevel; i++) {
      insertion -> next[i] = successors[i];
      predecessors[i] -> next[i] = insertion;
    }
    return 1;
  }
  else {
    __sync_fetch_and_sub(&dataLayer -> references, 1);
  }
  return 0;
}

//removes a value in the skip list when present
int removeNode(inode_t *sentinel, int val, node_t* dataLayer, int zone, memory_queue_t* garbage) {
  //store the result of a traversal through the skip list while searching for a value
  inode_t *predecessors[sentinel -> topLevel], *successors[sentinel -> topLevel];
  inode_t *previous = sentinel, *current = NULL;

  //Lazily searches the skip list, disregarding locks
  for (int i = sentinel -> topLevel - 1; i >= 0; i--) {
    current = previous -> next[i];
    while (current -> val < val) {
      previous = current;
      current = previous -> next[i];
    }
    predecessors[i] = previous;
    successors[i] = current;
  }

  //if the node contains the targeted value,
  //remove the node, decrement the data layer references value, and return true
  //otherwise return false
  int retval = 0;
  inode_t* candidate = current;
  if (candidate -> val == val) {
    for (int i = 0; i < candidate -> topLevel; i++) {
        predecessors[i] -> next[i] = successors[i] -> next[i];
    }
    mq_push(garbage, candidate);
    retval = 1;
  }
  
  __sync_fetch_and_sub(&dataLayer -> references, 1);
  if (validateRemoval(dataLayer)) {
    multi_push(mq[zone], dataLayer);
  }
  return retval;
}

#endif
