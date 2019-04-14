//Data Layer represents a linked list of real data elements
#ifndef DATA_LAYER_C
#define DATA_LAYER_C

#include "DataLayer.h"
#include "Atomic.h"
#include "LinkedList.h"
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#include <stdio.h>

dataLayerThread_t *remover = NULL;
pthread_t reclaimer;
volatile char stopGarbageCollection;
job_queue_t* garbage;
LinkedList_t* retiredList;

//Helper Functions
inline node_t* getElement(inode_t* sentinel, const int val, HazardNode_t* hazardNode);
inline void dispatchSignal(int val, node_t* dataLayer, Job operation);
inline int validateLink(node_t* previous, node_t* current);
inline int validateRemoval(node_t* previous, node_t* current);

inline node_t* getElement(inode_t* sentinel, const int val, HazardNode_t* hazardNode) {
	inode_t *previous = sentinel, *current = NULL;
	for (int i = previous -> topLevel - 1; i >= 0; i--) {
    hazardNode -> hp1 = previous -> next[i];
		current = (inode_t*)hazardNode -> hp1;
		while (current -> val < val) {
      hazardNode -> hp0 = current;
			previous = (inode_t*)hazardNode -> hp0;
      hazardNode -> hp1 = current -> next[i];
			current = (inode_t*)hazardNode -> hp1;
		}
	}
  hazardNode -> hp0 = previous -> dataLayer;
  hazardNode -> hp1 = NULL;
	return previous -> dataLayer;
}

inline void dispatchSignal(int val, node_t* dataLayer, Job operation) {
	assert(numaLayers != NULL);
	for (int i = 0; i < numberNumaZones; i++) {
		push(numaLayers[i] -> updates, val, operation, dataLayer);
	}
}

inline int validateLink(node_t* previous, node_t* current) {
	return previous -> next == current;
}

inline int validateRemoval(node_t* previous, node_t* current) {
  return previous -> next == current && current -> markedToDelete;
}

int lazyFind(searchLayer_t* numask, int val, HazardNode_t* hazardNode) {
	node_t* current = getElement(numask -> sentinel, val, hazardNode);
	while (current -> val < val) {
    hazardNode -> hp0 = current -> next;
		current = (node_t*)hazardNode -> hp0;
	}
	int retval = current -> val == val && current -> markedToDelete == 0;
  hazardNode -> hp0 = NULL;
  hazardNode -> hp1 = NULL;
  return retval;
}

int lazyAdd(searchLayer_t* numask, int val, HazardNode_t* hazardNode) {
	char retry = 1;
	while (retry) {
		node_t* previous = getElement(numask -> sentinel, val, hazardNode);
    hazardNode -> hp1 = previous -> next;
		node_t* current = (node_t*)hazardNode -> hp1;
		while (current -> val < val) {
      hazardNode -> hp0 = current;
			previous = (node_t*)hazardNode -> hp0;
      hazardNode -> hp1 = current -> next;
			current = (node_t*)hazardNode -> hp1;
		}
		pthread_mutex_lock(&previous -> lock);
		pthread_mutex_lock(&current -> lock);
    hazardNode -> hp0 = NULL;
    hazardNode -> hp1 = NULL;
		if (validateLink(previous, current)) {
      if (current -> val == val && current -> markedToDelete) {
        current -> markedToDelete = 0;
				current -> fresh = 1;
        pthread_mutex_unlock(&previous -> lock);
				pthread_mutex_unlock(&current -> lock);
        return 1;
      }
			else if (current -> val == val) {
				pthread_mutex_unlock(&previous -> lock);
				pthread_mutex_unlock(&current -> lock);
				return 0;
			}
			node_t* insertion = constructNode(val, numberNumaZones); //automatically set as fresh
			insertion -> next = current;
			previous -> next = insertion;
			pthread_mutex_unlock(&previous -> lock);
			pthread_mutex_unlock(&current -> lock);
			return 1;
		}
		pthread_mutex_unlock(&previous -> lock);
		pthread_mutex_unlock(&current -> lock);
	}
}

int lazyRemove(searchLayer_t* numask, int val, HazardNode_t* hazardNode) { //TODO: redo this method for proper locking
	char retry = 1;
	while (retry) {
		node_t* previous = getElement(numask -> sentinel, val, hazardNode);
    hazardNode -> hp1 = previous -> next;
		node_t* current = (node_t*)hazardNode -> hp1;
		while (current -> val < val) {
      hazardNode -> hp0 = current;
			previous = (node_t*)hazardNode -> hp0;
      hazardNode -> hp1 = current -> next;
			current = (node_t*)hazardNode -> hp1;
		}
		pthread_mutex_lock(&previous -> lock);
		pthread_mutex_lock(&current -> lock);
    hazardNode -> hp0 = NULL;
    hazardNode -> hp1 = NULL;
		if (validateLink(previous, current)) {
			if (current -> val != val || current -> markedToDelete) {
				pthread_mutex_unlock(&previous -> lock);
				pthread_mutex_unlock(&current -> lock);
				return 0;
			}
			current -> markedToDelete = 1;
			current -> fresh = 1;
			pthread_mutex_unlock(&previous -> lock);
			pthread_mutex_unlock(&current -> lock);
			return 1;
		}
		pthread_mutex_unlock(&previous -> lock);
		pthread_mutex_unlock(&current -> lock);
	}
}

void* backgroundRemoval(void* input) {
	dataLayerThread_t* thread = (dataLayerThread_t*)input;
	node_t* sentinel = thread -> sentinel;
	while (thread -> finished == 0) {
		usleep(thread -> sleep_time);
		node_t* previous = sentinel;
		node_t* current = sentinel -> next;
		while (current -> next != NULL) {
			if (current -> fresh) {
				current -> fresh = 0; //unset as fresh, need a CAS here? only thread operating on structure
				if (current -> markedToDelete) {
					dispatchSignal(current -> val, current, REMOVAL);
				}
				else {
					dispatchSignal(current -> val, current, INSERTION);
				}
			}
			else if (current -> markedToDelete && current -> references % 4 == 0) {
				int valid = 0;
				pthread_mutex_lock(&previous -> lock);
				pthread_mutex_lock(&current -> lock);
				if ((valid = validateRemoval(previous, current)) != 0) {
					previous -> next = current -> next;
					push(garbage, current -> val, MEMORY_RECLAMATION, current);
				}
				pthread_mutex_unlock(&previous -> lock);
				pthread_mutex_unlock(&current -> lock);
				if (valid) {
					current = current -> next;
					continue;
				}
			}
			previous = current;
			current = current -> next;
		}
	}
	return NULL;
}

static dataLayerThread_t* constructDataLayerThread() {
	dataLayerThread_t* thread = (dataLayerThread_t*)malloc(sizeof(dataLayerThread_t));
	thread -> running = 0;
	thread -> finished = 0;
	thread -> sleep_time = 10000;
	thread -> sentinel = NULL;
	return thread;
}

static dataLayerThread_t* destructDataLayerThread(dataLayerThread_t* thread) {
	free(thread);
}

void startDataLayer(node_t* sentinel) {
	if (remover == NULL) {
		remover = constructDataLayerThread();
	}
	if (remover -> running == 0) {
		stopGarbageCollection = 0;
		pthread_create(&reclaimer, NULL, garbageCollectDataLayer, NULL);
		remover -> finished = 0;
		remover -> sentinel = sentinel;
		pthread_create(&remover -> runner, NULL, backgroundRemoval, (void*)remover);
		remover -> running = 1;
	}
}

void stopDataLayer() {
	if (remover -> running) {
		remover -> finished = 1;
		pthread_join(remover -> runner, NULL);
		stopGarbageCollection = 1;
		pthread_join(reclaimer, NULL);
		remover -> running = 0;
		destructDataLayerThread(remover);
	}
}

inline void collectGarbage(job_queue_t* garbage, LinkedList_t* retiredList) {
  q_node_t* job;
  while ((job = pop(garbage)) != NULL) {
    RETIRE_NODE(retiredList, job -> node);
    free(job);
  }
}

void* garbageCollectDataLayer(void* args) {
  //Instantiate Retired List and Garbage
  retiredList = constructLinkedList();
  garbage = constructJobQueue();

	while (stopGarbageCollection == 0) {
    usleep(1000);
    collectGarbage(garbage, retiredList);
  }
  collectGarbage(garbage, retiredList);
  destructJobQueue(garbage);
  destructLinkedList(retiredList);
}

#endif
