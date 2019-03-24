#ifndef SKIPLISTLAZYLOCK_H
#define SKIPLISTLAZYLOCK_H

#include "Nodes.h"
#include "JobQueue.h"

extern searchLayer_t** numaLayers;

//driver functions
int add(inode_t *sentinel, int val, node_t* dataLayer, int zone);
int removeNode(inode_t *sentinel, int val, int zone);

//helper functions
void destructIndexSkipList(inode_t* sentinel);

#endif
