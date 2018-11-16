#ifndef JOB_QUEUE_H_
#define JOB_QUEUE_H_

#include "SkipList.h"

typedef enum job {
	INSERTION,
	REMOVAL,
	NONE
} Job;

typedef struct q_node {
	int val;
	Job operation;
	struct q_node* next;
} q_node_t;

q_node_t* constructQNode(int val, Job operation);

typedef struct job_queue {
	q_node_t* head;
	q_node_t* tail;
	q_node_t* sentinel;
} job_queue_t;

job_queue_t* constructJobQueue();
void destructJobQueue(job_queue_t* jobs);
void push(job_queue_t* jobs, int val, Job operation);
q_node_t* pop(job_queue_t* jobs);


#endif
