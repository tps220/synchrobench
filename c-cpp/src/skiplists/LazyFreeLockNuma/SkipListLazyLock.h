#ifndef SKIPLISTLAZYLOCK_H
#define SKIPLISTLAZYLOCK_H

#include "Nodes.h"
#include "JobQueue.h"

//driver functions
int add(inode_t *sentinel, int val, node_t* dataLayer, int zone);
int removeNode(inode_t *sentinel, int val, int zone, job_queue_t* garbage);

#endif
