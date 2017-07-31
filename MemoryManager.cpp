
#include <zconf.h>
#include <vector>
#include <map>
#include "MemoryManager.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


#define MAX_INT32 2147483647


struct Bounds{

	uint64 start;

	size_t size;
};

typedef std::map<uint64,Bounds> allocMap;
typedef std::vector<Bounds> freeVector;

uint64 allocCounter = 0;

pthread_mutex_t *allocCounterMutex;

allocMap allocatedMemoryMap;

pthread_mutex_t *allocatedMemoryMapMutex;

freeVector freeMemoryVector;

pthread_mutex_t *freeMemoryVectorMutex;

uint64 lastGapEndAddress = (MAX_INT32 *(pow(2,4)));

uint64 lastGapStartAddress;

pthread_mutex_t *lastGapStartAddressMutex;

bool firstAllocation = true;

void manager_init(){


	//initiate the mutexes
	pthread_mutex_init(allocCounterMutex, NULL);
	pthread_mutex_init(allocatedMemoryMapMutex, NULL);
	pthread_mutex_init(lastGapStartAddressMutex, NULL);
	pthread_mutex_init(freeMemoryVectorMutex, NULL);
}

void manager_fini(){
	//destroy mutexes
	pthread_mutex_destroy(allocCounterMutex);
	pthread_mutex_destroy(allocatedMemoryMapMutex);
	pthread_mutex_destroy(lastGapStartAddressMutex);
	pthread_mutex_destroy(freeMemoryVectorMutex);

	allocatedMemoryMap.clear();
	freeMemoryVector.clear();
}

/**
 * insert the allocation information to the allocatedMemoryMap
 * @param allocatedPages a Bounds structure that contain the information of the given allocation
 */
void allocatedPagesInsertion(struct Bounds *allocatedPages){
	//insret to the allocatedMemoryMap
	pthread_mutex_lock(allocatedMemoryMapMutex);
	allocatedMemoryMap[allocCounter]= (*allocatedPages);
	allocCounter+=1;
	pthread_mutex_unlock(allocatedMemoryMapMutex);
};

uint64 manager_malloc(size_t size){
	bool flag =false;
	if(size <= 0){
		return 0;
	}

	//search for free pages big enough for the allocation needed
	// and within the segment that already being allocated
	uint64 pageStart = firstFit(size, &flag);
	if( flag){
		Bounds allocatedPages = {pageStart,size};
		allocatedPagesInsertion(&allocatedPages);

	}else{
		pthread_mutex_lock(lastGapStartAddressMutex);
		if((lastGapEndAddress - lastGapStartAddress) >= size) {
			//creating a new bounds that contains all of the information on the allocation at hand
			Bounds allocatedPages = {lastGapStartAddress,size};
			lastGapStartAddress += size;
			pthread_mutex_unlock(lastGapStartAddressMutex);

			allocatedPagesInsertion(&allocatedPages);
		}
		pthread_mutex_unlock(lastGapStartAddressMutex);
	}
}

void manager_free(uint64 alloc_id){
	//search for allocated memory
	pthread_mutex_lock(allocatedMemoryMapMutex);
	allocMap::iterator search = allocatedMemoryMap.find(alloc_id);
	if(search != allocatedMemoryMap.end()){

		Bounds freePages = search->second;
		allocatedMemoryMap.erase(search);
		pthread_mutex_unlock(allocatedMemoryMapMutex);
		//todo add it the freeMemoryPages

	}else{
		pthread_mutex_unlock(allocatedMemoryMapMutex);
	}
}
