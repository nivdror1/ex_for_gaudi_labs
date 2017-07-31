
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

unsigned int nextFitIndex = 0;

pthread_mutex_t *nextFitIndexMutex;

void manager_init(){


	//initiate the mutexes
	pthread_mutex_init(allocCounterMutex, NULL);
	pthread_mutex_init(allocatedMemoryMapMutex, NULL);
	pthread_mutex_init(lastGapStartAddressMutex, NULL);
	pthread_mutex_init(freeMemoryVectorMutex, NULL);
	pthread_mutex_init(nextFitIndexMutex, NULL);
}

void manager_fini(){
	//destroy mutexes
	pthread_mutex_destroy(allocCounterMutex);
	pthread_mutex_destroy(allocatedMemoryMapMutex);
	pthread_mutex_destroy(lastGapStartAddressMutex);
	pthread_mutex_destroy(freeMemoryVectorMutex);
	pthread_mutex_destroy(nextFitIndexMutex);

	allocatedMemoryMap.clear();
	freeMemoryVector.clear();
}

/**
 * insert the allocation information to the allocatedMemoryMap
 * @param allocatedPages a Bounds structure that contain the information of the given allocation
 */
void allocatedPagesInsertion(struct Bounds *allocatedPages){
	//insert to the allocatedMemoryMap
	pthread_mutex_lock(allocatedMemoryMapMutex);
	allocatedMemoryMap[allocCounter] = (*allocatedPages);
	allocCounter ++;
	pthread_mutex_unlock(allocatedMemoryMapMutex);
};

/**
 * search the freeMemoryVector for a memory hole that fit size that being allocated.
 * @param size the size that the user want to allocate
 * @param found a boolean variable that signify if we found a suitable memory hole
 * @return the start address of allocated memory
 */
uint64 firstFit(size_t size, bool *found){
	//todo a problem with synchronization of the index between threads
	uint64 startAddress = 0;

	pthread_mutex_lock(nextFitIndexMutex);
	unsigned int currindex = nextFitIndex;
	pthread_mutex_unlock(nextFitIndexMutex);

	pthread_mutex_lock(freeMemoryVectorMutex);
	unsigned long vectorSize = freeMemoryVector.size();
	pthread_mutex_unlock(freeMemoryVectorMutex);

	//search for the a memory allocation that fit the size
	while(currindex!=vectorSize && (*found)){

		Bounds segment = freeMemoryVector.at(nextFitIndex);

		if(segment.size >= size){

			(*found)=true;
			startAddress = segment.start;
			segment.size -= size;
			segment.start += size;

			//delete the pages from the vector iff there size is zero
			if(segment.size == 0){
				freeMemoryVector.erase(freeMemoryVector.begin()+nextFitIndex);
			}
		}
		nextFitIndex++;
	}
	//restarting the nextFitIndex back to zero
	pthread_mutex_lock(nextFitIndexMutex);
	if(nextFitIndex ==freeMemoryVector.size()){
		nextFitIndex = 0;
	}
	pthread_mutex_unlock(nextFitIndexMutex);
	return startAddress;

}

uint64 manager_malloc(size_t size){
	bool found =false;
	if(size <= 0){
		return 0;
	}

	//search for free pages big enough for the allocation needed
	// and within the segment that already being allocated
	uint64 pageStart = firstFit(size, &found);
	if( found){
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
		}else{
			pthread_mutex_unlock(lastGapStartAddressMutex);
			return 0;
		}

	}
	return (uint64)size;
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
