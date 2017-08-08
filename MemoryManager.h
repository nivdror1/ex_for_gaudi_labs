
#ifndef GAUDILABS_MEMORYMANAGER_H
#define GAUDILABS_MEMORYMANAGER_H


#include <stdio.h>
#include <stdint.h>

/**
 * initiate the library
 */
void manager_init();

/**
 * terminate the library
 */
void manager_fini();

/**
 * manage the allocation
 * @param size the size was the allocation requested
 * @return the size that was allocated
 */
uint64_t manager_malloc(size_t size);

/**
 * free the requested memory
 * @param alloc_id the allocation id
 */
void manager_free(uint64_t alloc_id);

#endif //GAUDILABS_MEMORYMANAGER_H
