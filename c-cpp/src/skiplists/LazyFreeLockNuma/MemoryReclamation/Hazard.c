//Thomas Salemy

#ifndef HAZARD_C
#define HAZARD_C
#include "Hazard.h"
#include "Nodes.h"
#include "Atomic.h"

HazardNode_t* constructHazardNode(int zone) {
    HazardNode_t* node = (HazardNode_t*)nalloc(allocators[zone], sizeof(HazardNode_t));
    node -> hp0 = node -> hp1 = NULL;
    node -> next = NULL;
    return node;
}

void destructHazardNode(HazardNode_t* node) {
  free(node);
}

HazardContainer_t* constructHazardContainer(HazardNode_t* head, int H) {
    HazardContainer_t* container = (HazardContainer_t*)malloc(sizeof(HazardContainer_t));
    container -> head = head;
    container -> H = H;
    return container;
}

void destructHazardContainer(HazardContainer_t* container) {
  HazardNode_t* runner = container -> head;
  while (runner != NULL) {
    HazardNode_t* temp = runner;
    runner = runner -> next;
    destructHazardNode(temp);
  }
  free(container);
}

void retireElement(LinkedList_t* retiredList, void* ptr, void (*reclaimMemory)(void*)) {
    ll_push(retiredList, ptr);
    if (retiredList -> size >= MAX_DEPTH) {
      scan(retiredList, reclaimMemory);
    }
}

void scan(LinkedList_t* retiredList, void (*reclaimMemory)(void*)) {
    //Collect all valid hazard pointers across application threads
    LinkedList_t* ptrList = constructLinkedList();
    HazardNode_t* runner = memoryLedger -> head;
    while (runner != NULL) {
        if (runner -> hp0 != NULL) {
            ll_push(ptrList, runner -> hp0);
        }
        if (runner -> hp1 != NULL) {
            ll_push(ptrList, runner -> hp1);
        }
        runner = runner -> next;
    }

    //Compare retired candidates against active hazard nodes, reclaiming or procastinating
    int listSize = retiredList -> size;
    void** tmpList = (void**)malloc(listSize * sizeof(void*));
    ll_popAll(retiredList, tmpList);
    for (int i = 0; i < listSize; i++) {
        if (findElement(ptrList, tmpList[i])) {
            ll_push(retiredList, tmpList[i]);
        }
        else {
            reclaimMemory(tmpList[i]);
        }
    }
    free(tmpList);
}

void reclaimIndexNode(void* ptr) {
    inode_t* node = (inode_t*)ptr;
    FAD(&node -> dataLayer -> references);
    free(node);
}

void reclaimDataLayerNode(void* ptr) {
    free(ptr);
}

#endif
