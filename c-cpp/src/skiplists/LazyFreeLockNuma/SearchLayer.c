#ifndef SEARCH_LAYER_C
#define SEARCH_LAYER_C

#define _GNU_SOURCE
#include "SearchLayer.h"
#include "SkipListLazyLock.h"
#include "JobQueue.h"
#include "LinkedList.h"
#include "Hazard.h"
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <numaif.h>
#include <atomic_ops.h>
#include <numa.h>
#include <sched.h>

#include <stdio.h>

searchLayer_t* constructSearchLayer(inode_t* sentinel, int zone) {
	searchLayer_t* numask = (searchLayer_t*)malloc(sizeof(searchLayer_t));
	numask -> sentinel = sentinel;
	numask -> numaZone = zone;
	numask -> updates = constructJobQueue();
	numask -> garbage = constructJobQueue();
	numask -> finished = 0;
	numask -> stopGarbageCollection = 0;
	numask -> running = 0;
	numask -> sleep_time = 0;
	return numask;
}

searchLayer_t* destructSearchLayer(searchLayer_t* numask) {
	stopIndexLayer(numask);
	destructJobQueue(numask -> updates);
	destructJobQueue(numask -> garbage);
	inode_t* runner = numask -> sentinel;
  while (runner != NULL) {
    inode_t* temp = runner;
    runner = runner -> next[0];
    destructIndexNode(temp, numask -> numaZone);
  }
	free(numask);
}

int searchLayerSize(searchLayer_t* numask) {
	inode_t* runner = numask -> sentinel;
	int size = -2;
	while (runner != NULL) {
		size++;
		runner = runner -> next[0];
	}
	return size;
}

void startIndexLayer(searchLayer_t* numask, int sleep_time) {
	numask -> sleep_time = sleep_time;
	if (numask -> running == 0) {
    numask -> stopGarbageCollection = 0;
    numask -> finished = 0;
		pthread_create(&numask -> reclaimer, NULL, garbageCollectionIndexLayer, (void*)numask);
		pthread_create(&numask -> updater, NULL, updateNumaZone, (void*)numask);
    numask -> running = 1;
	}
}

void stopIndexLayer(searchLayer_t* numask) {
	if (numask -> running) {
		numask -> finished = 1;
		pthread_join(numask -> updater, NULL);
		numask -> stopGarbageCollection = 1;
    pthread_join(numask -> reclaimer, NULL);
		numask -> running = 0;
	}
}

void* updateNumaZone(void* args) {
	searchLayer_t* numask = (searchLayer_t*)args;
	job_queue_t* updates = numask -> updates;
	job_queue_t* garbage = numask -> garbage;
	inode_t* sentinel = numask -> sentinel;
	const int numaZone = numask -> numaZone;

	//Pin to Zone & CPU
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(numaZone, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	while (numask -> finished == 0) {
		usleep(numask -> sleep_time);
		while (numask -> finished == 0 && runJob(sentinel, pop(updates), numask -> numaZone, garbage)) {}
	}

	return NULL;
}

int runJob(inode_t* sentinel, q_node_t* job, int zone, job_queue_t* garbage) {
	if (job == NULL) {
		return 0;
	}
	else if (job -> operation == INSERTION) {
		add(sentinel, job -> val, (node_t*)job -> node, zone);
	}
	else if (job -> operation == REMOVAL) {
		removeNode(sentinel, job -> val, zone, garbage);
	}
	free(job);
	return 1;
}

inline void collectGarbage(job_queue_t* garbage, LinkedList_t* retiredList, int zone) {
  q_node_t* job;
  while ((job = pop(garbage)) != NULL) {
    RETIRE_INDEX_NODE(retiredList, job -> node, zone);
    free(job);
  }
}

void* garbageCollectionIndexLayer(void* args) {
  searchLayer_t* numask = (searchLayer_t*)args;
  job_queue_t* garbage = numask -> garbage;
  const int zone = numask -> numaZone;

	//Pin to Zone & CPU
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(numask -> numaZone, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  //Instantiate Retired List
  LinkedList_t* retiredList = constructLinkedList();

	while (numask -> stopGarbageCollection == 0) {
    usleep(numask -> sleep_time);
    collectGarbage(garbage, retiredList, zone);
  }
  collectGarbage(garbage, retiredList, zone);
  //destruct linked list
  int listSize = retiredList -> size;
  void** tmpList = (void**)malloc(listSize * sizeof(void*));
  ll_pipeAndRemove(retiredList, tmpList);
  for (int i = 0; i < listSize; i++) {
  	destructIndexNode((inode_t*)tmpList[i], zone);
  }
	free(retiredList);
}


#endif
