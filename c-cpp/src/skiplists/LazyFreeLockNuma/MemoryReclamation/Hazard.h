#ifndef HAZARD_H
#define HAZARD_H

#include "Allocator.h"

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
} HazardContainer_t;

HazardContainer_t* constructHazardContainer(HazardNode_t* head);
void destructHazardContainer(HazardContainer_t* container);

extern HazardContainer_t* memoryLedger;

#endif
