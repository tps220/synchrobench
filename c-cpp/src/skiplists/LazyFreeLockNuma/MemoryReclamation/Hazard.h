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
void destructHazardNode(HazardNode_t* node, int zone);

typedef struct HazardContainer {
    HazardNode_t* head;
    int H;
} HazardContainer_t;

HazardContainer_t* constructHazardContainer(HazardNode_t* head, int H);
void destructHazardContainer(HazardContainer_t* container);

extern HazardContainer_t* memoryLedger;

void retireElement(LinkedList_t* hazardNode, void* ptr, int zone, void (*reclaimMemory)(void*, int));
void scan(LinkedList_t* hazardNode, void (*reclaimMemory)(void*, int), int zone);
void reclaimIndexNode(void* ptr, int zone);
void reclaimDataLayerNode(void* ptr, int zone);

#define RETIRE_INDEX_NODE(retiredList, ptr, zone) retireElement((retiredList), (ptr), (zone), reclaimIndexNode)
#define RETIRE_NODE(retiredList, ptr) retireElement((retiredList), (ptr), 0, reclaimDataLayerNode)

#endif
