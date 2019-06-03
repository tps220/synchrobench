#ifndef MULTI_QUEUE_C
#define MULTI_QUEUE_C

#include "MultiQueue.h"

multi_node_t* constructMultiNode(node_t* target) {
	multi_node_t* new_job = (multi_node_t*)malloc(sizeof(multi_node_t));
	new_job -> target = target;
	new_job -> next = NULL;
	return new_job;
}

multi_queue_t* constructMultiQueue() {
	multi_queue_t* jobs = (multi_queue_t*)malloc(sizeof(multi_queue_t));
	jobs -> sentinel = constructMultiNode(NULL);
	jobs -> tail = jobs -> head = jobs -> sentinel;
	return jobs;
}

void destructMultiQueue(multi_queue_t* jobs) {
	multi_node_t* runner = jobs -> head;
	while (runner != NULL) {
		multi_node_t* temp = runner;
		runner = runner -> next;
		free(temp);
	}
	free(jobs);
}

void multi_push(multi_queue_t* jobs, node_t* target) {
	if (__sync_fetch_and_or(&target -> inQueue, 1)) {
		return;
	}
	multi_node_t* new_job = constructMultiNode(target);
	jobs -> tail -> next = new_job;
	jobs -> tail = new_job;
}

multi_node_t* multi_pop(multi_queue_t* jobs) {
	if (jobs -> head -> next == NULL) {
		return NULL;
	}
	multi_node_t* front = jobs -> head;
	jobs -> head = jobs -> head -> next;
  front -> target = jobs -> head -> target;
	return front;
}

#endif
