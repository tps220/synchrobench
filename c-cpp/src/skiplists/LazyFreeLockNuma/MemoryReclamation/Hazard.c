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

void destructHazardNode(HazardNode_t* node, int zone) {
  nfree(allocators[zone], node, sizeof(HazardNode_t));
}

HazardContainer_t* constructHazardContainer(HazardNode_t* head) {
    HazardContainer_t* container = (HazardContainer_t*)malloc(sizeof(HazardContainer_t));
    container -> head = head;
    return container;
}

void destructHazardContainer(HazardContainer_t* container) {
  free(container);
}

#endif
