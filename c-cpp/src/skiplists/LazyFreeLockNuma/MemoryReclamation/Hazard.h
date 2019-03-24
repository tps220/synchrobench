//Thomas Salemy

#ifndef HAZARD_H
#define HAZARD_H

#include "LinkedList.h"
#include "Allocator.h"

#define MAX_DEPTH 5
extern numa_allocator_t** allocators;

typedef struct HazardNode {
    void* hp0;
    void* hp1;
    struct HazardNode* next;
} HazardNode_t;

HazardNode_t* constructHazardNode(int zone);
void destructHazardNode(HazardNode_t* node);

typedef struct HazardContainer {
    HazardNode_t* head;
    int H;
} HazardContainer_t;

HazardContainer_t* constructHazardContainer(HazardNode_t* head, int H);
void destructHazardContainer(HazardContainer_t* container);

extern HazardContainer_t* memoryLedger;

void retireElement(HazardNode_t* hazardNode, void* ptr, void (*reclaimMemory)(void*));
void scan(HazardNode_t* hazardNode, void (*reclaimMemory)(void*));
void reclaimIndexNode(void* ptr);
void reclaimDataLayerNode(void* ptr);

#define RETIRE_INDEX_NODE(retiredList, ptr) retireElement((retiredList), (ptr), reclaimIndexNode)
#define RETIRE_NODE(retiredList, ptr) retireElement((retiredList), (ptr), reclaimDataLayerNode)

#endif
