#ifndef DATA_LAYER_C
#define DATA_LAYER_C

#include "DataLayer.h"
#include "Atomic.h"
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

//Update helpers
dataLayerThread_t* remover = NULL;
dataLayerThread_t* updater = NULL;
//Gargabe Collection helper
gc_container_t* gc = NULL;
//Deletion helper
multi_queue_t** mq = NULL;

//Helper Functions
inline node_t* getElement(inode_t* sentinel, const int val, HazardNode_t* hazardNode);
inline void dispatchSignal(int val, node_t* dataLayer, Job operation);
inline int validateLink(node_t* previous, node_t* current);
inline int validateRemoval(node_t* previous, node_t* current);
inline void collect(memory_queue_t* garbage, LinkedList_t* retiredList);

inline dataLayerThread_t* constructDataLayerThread();

inline node_t* getElement(inode_t* sentinel, const int val, HazardNode_t* hazardNode) {
  inode_t *previous = sentinel, *current = NULL;
  for (int i = previous -> topLevel - 1; i >= 0; i--) {
    hazardNode -> hp1 = previous -> next[i];
    current = (inode_t*)hazardNode -> hp1;
    while (current -> val < val) {
      hazardNode -> hp0 = current;
      previous = (inode_t*)hazardNode -> hp0 ;
      hazardNode -> hp1 = current -> next[i];
      current = (inode_t*)hazardNode -> hp1;
    }
  }
  hazardNode -> hp0 = previous -> dataLayer;
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
  return previous -> next == current && 
         current -> previous == previous &&
         current -> markedToDelete && 
         current -> references == 0;
}

int lazyFind(searchLayer_t* numask, int val, HazardNode_t* hazardNode) {
  node_t* previous = getElement(numask -> sentinel, val, hazardNode);
  hazardNode -> hp1 = previous -> next;
  node_t* current = (node_t*)hazardNode -> hp1;
  while (current -> val < val) {
    hazardNode -> hp0 = current;
    previous = (node_t*)hazardNode -> hp0;
    hazardNode -> hp1 = current -> next;
    current = (node_t*)hazardNode -> hp1;
  }
  int found = current -> val == val && current -> markedToDelete == 0;
  hazardNode -> hp0 = NULL;
  hazardNode -> hp1 = NULL;
  return found;
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
    if (validateLink(previous, current) && previous -> markedToDelete != 2) {
      if (current -> val == val && current -> markedToDelete) {
        current -> markedToDelete = 0;
        current -> fresh = 2;
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
      insertion -> previous = previous;

      current -> previous = insertion;
      previous -> next = insertion;

      pthread_mutex_unlock(&previous -> lock);
      pthread_mutex_unlock(&current -> lock);
      return 1;
    }
    pthread_mutex_unlock(&previous -> lock);
    pthread_mutex_unlock(&current -> lock);
  }
}

int lazyRemove(searchLayer_t* numask, int val, HazardNode_t* hazardNode) {
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
      if (validateRemoval(previous, current)) {
        current -> fresh = 0;
        multi_push(mq[numberNumaZones + hazardNode -> id], current);
      }
      else {
        current -> fresh = 1;
      }
      pthread_mutex_unlock(&previous -> lock);
      pthread_mutex_unlock(&current -> lock);
      return 1;
    }
    pthread_mutex_unlock(&previous -> lock);
    pthread_mutex_unlock(&current -> lock);
  }
}

/*
void updates(multi_queue_t* queue) {
  multi_node_t* consumer = multi_pop(queue);
  if (consumer) {
    node_t* target = consumer -> target;
    if (target -> fresh) {
      target -> fresh = 0;
      if (target -> markedToDelete) {
        target -> attempts++;
        dispatchSignal(target -> val, target, REMOVAL);
      }
      else {
        dispatchSignal(target -> val, target, INSERTION);
      }
    }
  }
}
*/

void* backgroundPropogation(void* input) {
  dataLayerThread_t* thread = (dataLayerThread_t*)input;
  node_t* sentinel = thread -> sentinel;
  while (thread -> finished == 0) {
    usleep(thread -> sleep_time);
    node_t* runner = sentinel -> next;
    while (runner -> next != NULL) {
      if (runner -> fresh) {
        runner -> fresh = 0;
        if (runner -> markedToDelete) {
          dispatchSignal(runner -> val, runner, REMOVAL);
        }
        else {
          runner -> references += numberNumaZones;
          dispatchSignal(runner -> val, runner, INSERTION);
        }
      }
      runner = runner -> next;
    }
  }
  return NULL;
}

int removal(multi_queue_t *queue) {
  multi_node_t* consumer = multi_pop(queue);
  if (consumer) {
    node_t* target = consumer -> target;
    node_t* previous = target -> previous;

    pthread_mutex_lock(&previous -> lock);
    pthread_mutex_lock(&target -> lock);
    int valid;
    if ((valid = validateRemoval(previous, target)) != 0) {
      target -> markedToDelete = 2;
      previous -> next = target -> next;
      target -> next -> previous = previous;
    }
    pthread_mutex_unlock(&previous -> lock);
    pthread_mutex_unlock(&target -> lock);
    if (valid) {
      mq_push(gc -> garbage, target);
    }
    else {
      multi_push(mq[numberNumaZones + numThreads], target);
    }
    free(consumer);
    return 1;
  }
  free(consumer);
  return 0;
}

void* backgroundRemoval(void* input) {
  dataLayerThread_t* thread = (dataLayerThread_t*)input;
  while (thread -> finished == 0) {
    for (int i = 0; i < numberNumaZones + numThreads + 1; i++) {
      removal(mq[i]);
    }
  }
  return NULL;
}

inline void constructMQs() {
  mq = (multi_queue_t**)malloc((numberNumaZones + numThreads + 1) * sizeof(multi_queue_t*));
  for (int i = 0; i < numberNumaZones + numThreads + 1; i++) {
    mq[i] = constructMultiQueue();
  }
}

inline void destructMQs() {
  for (int i = 0; i < numberNumaZones + numThreads + 1; i++) {
    destructMultiQueue(mq[i]);
  }
  free(mq);
}


inline dataLayerThread_t* constructDataLayerThread() {
  dataLayerThread_t* thread = (dataLayerThread_t*)malloc(sizeof(dataLayerThread_t));
  thread -> running = 0;
  thread -> finished = 0;
  thread -> sleep_time = 100;
  thread -> sentinel = NULL;
  return thread;
}

inline void destructDataLayerThread(dataLayerThread_t* thread) {
  free(thread);
  thread = NULL;
}

inline gc_container_t* construct_gc_container() {
  gc_container_t* container = (gc_container_t*)malloc(sizeof(gc_container_t));
  container -> garbage = constructMemoryQueue();
  container -> retiredList = constructLinkedList();
  container -> stopGarbageCollection = 0;
  return container;
}

inline void destruct_gc_container(gc_container_t* gc) {
  destructMemoryQueue(gc -> garbage);
  destructLinkedList(gc -> retiredList, 1);
  free(gc);
}

void startDataLayerHelpers(node_t* sentinel) {
  updater = constructDataLayerThread();
  gc = construct_gc_container();
  remover = constructDataLayerThread();
  constructMQs();
  if (updater -> running == 0) {
    gc -> stopGarbageCollection = 0;
    pthread_create(&gc -> reclaimer, NULL, garbageCollectDataLayer, (void*)gc);

    updater -> finished = 0;
    updater -> sentinel = sentinel;
    pthread_create(&updater -> runner, NULL, backgroundPropogation, (void*)updater);
    pthread_create(&remover -> runner, NULL, backgroundRemoval, (void*)remover);
    updater -> running = 1;
  }
}

void stopDataLayerHelpers() {
  if (updater -> running) {
    updater -> finished = 1;
    pthread_join(updater -> runner, NULL);
    updater -> running = 0;
    destructDataLayerThread(updater);

    remover -> finished = 1;
    pthread_join(remover -> runner, NULL);
    remover -> running = 0;
    destructDataLayerThread(remover);
    destructMQs();

    gc -> stopGarbageCollection = 1;
    pthread_join(gc -> reclaimer, NULL);
    destruct_gc_container(gc);
  }
}

void* garbageCollectDataLayer(void* args) {
  gc_container_t* gc = (gc_container_t*)args;

  while (gc -> stopGarbageCollection == 0) {
    usleep(10);
    collect(gc -> garbage, gc -> retiredList);
  }
  collect(gc -> garbage, gc -> retiredList);
}

inline void collect(memory_queue_t* garbage, LinkedList_t* retiredList) {
  m_node_t* subscriber;
  while ((subscriber = mq_pop(garbage)) != NULL) {
    RETIRE_NODE(retiredList, subscriber -> node);
    free(subscriber);
  }
}
#endif
