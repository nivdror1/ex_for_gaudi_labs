
#ifndef GAUDILABS_MEMORYMANAGER_H
#define GAUDILABS_MEMORYMANAGER_H


#include <stdio.h>
#include <stdint.h>


void manager_init();

void manager_fini();

uint64_t manager_malloc(size_t size);

void manager_free(uint64_t alloc_id);

#endif //GAUDILABS_MEMORYMANAGER_H
