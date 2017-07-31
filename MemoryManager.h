
#ifndef GAUDILABS_MEMORYMANAGER_H
#define GAUDILABS_MEMORYMANAGER_H


#include <stdio.h>
#include <tiff.h>

void manager_init();

void manager_fini();

uint64 manager_malloc(size_t size);

void manager_free(uint64 alloc_id);

#endif //GAUDILABS_MEMORYMANAGER_H
