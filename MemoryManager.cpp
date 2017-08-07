
#include <vector>
#include <map>
#include "MemoryManager.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>


#define MAX_INT32 2147483647
#define BYTES_2MB 2097152


struct Bounds{

	uint64_t start;

	size_t size;
};

typedef std::map<uint64_t,Bounds> allocMap;
typedef std::vector<Bounds> freeVector;

/** the allocation id counter*/
uint64_t allocCounter = 0;

/** A map of the allocated memory*/
allocMap allocatedMemoryMap;

/** A mutex for the allocatedMemoryMap*/
pthread_mutex_t *allocatedMemoryMapMutex;

/** A vector of the free memory*/
freeVector freeMemoryVector;

/** A mutex of the freeMemoryVector*/
pthread_mutex_t *freeMemoryVectorMutex;

/** The end address of the memory sequence*/
uint64_t lastGapEndAddress;

/** the start address of the memory that haven't been allocated yet*/
uint64_t lastGapStartAddress;

/** A mutex for the lastGapStartAddress*/
pthread_mutex_t *lastGapStartAddressMutex;


/**
 * create a non specific mutex
 * @param mutex a mutex
 */
void createMutex(pthread_mutex_t *mutex){
	if(pthread_mutex_init(mutex,NULL)!=0){
		printf("failure: pthread_mutex_init failed\n");
		exit(1);
	}
}

/**
 * lock the mutex
 * @param mutex a mutex
 */
void lockMutex(pthread_mutex_t *mutex){
	if(pthread_mutex_lock(mutex)!=0){
		printf("failure: pthread_mutex_lock failed\n");
		exit(1);
	}
}

/**
 * unlock the mutexd
 * @param mutex a mutex
 */
void unlockMutex(pthread_mutex_t *mutex){
	if(pthread_mutex_unlock(mutex)!=0){
		printf("failure: pthread_mutex_unlock failed\n");
		exit(1);
	}
}

/**
 * destroy a mutex
 * @param mutex a  mutex
 */
void destroyMutex(pthread_mutex_t *mutex){
	if(pthread_mutex_destroy(mutex)!=0){
		printf("failure: pthread_mutex_destroy failed\n");
		exit(1);
	}
}

/**
 * initiate the library
 */
void manager_init(){

	lastGapEndAddress = (MAX_INT32 *(uint64_t)(pow(2,4)));
	//initiate the mutexes
	createMutex(allocatedMemoryMapMutex);
	createMutex(lastGapStartAddressMutex);
	createMutex(freeMemoryVectorMutex);
}

/**
 * terminate the library
 */
void manager_fini(){
	//destroy mutexes
	destroyMutex(allocatedMemoryMapMutex);
	destroyMutex(lastGapStartAddressMutex);
	destroyMutex(freeMemoryVectorMutex);

	lastGapEndAddress = lastGapStartAddress = 0;

	allocatedMemoryMap.clear();
	freeMemoryVector.clear();
}

/**
 * insert the allocation information to the allocatedMemoryMap
 * @param allocatedPages a Bounds structure that contain the information of the given allocation
 */
void allocatedPagesInsertion(struct Bounds *allocatedPages){
	//insert to the allocatedMemoryMap
	lockMutex(allocatedMemoryMapMutex);
	allocatedMemoryMap[allocCounter] = (*allocatedPages);
	allocCounter ++;
	unlockMutex(allocatedMemoryMapMutex);
};

/**
 * search the freeMemoryVector for a memory hole that fit size that being allocated.
 * @param size the size that the user want to allocate
 * @param found a boolean variable that signify if we found a suitable memory hole
 * @return the start address of allocated memory
 */
uint64_t firstFit(size_t size, bool *found){
	uint64_t startAddress = 0;
	lockMutex(freeMemoryVectorMutex);

	freeVector::iterator iter = freeMemoryVector.begin();
	//search for the a memory allocation that fit the size
	for(iter; iter != freeMemoryVector.end();++iter ){
		//check if the size of the current page is sufficient
		if((*iter).size >= size){
			startAddress = (*iter).start;

			//delete the pages from the vector iff there size is zero
			if((*iter).size - size  == 0){
				freeMemoryVector.erase(iter);
			}else{
				(*iter).size -= size;
				(*iter).start += size;
			}
			unlockMutex(freeMemoryVectorMutex);
			//noted that a suitable allocation has been found
			(*found)=true;
			return startAddress;
		}
	}
	unlockMutex(freeMemoryVectorMutex);
	return startAddress;
}
/**
 * manage the allocation
 * @param size the size was the allocation requested
 * @return the size that was allocated
 */
uint64_t manager_malloc(size_t size){
	bool found =false;
	if(size <= 0){
		return 0;
	}

	//search for free pages big enough for the allocation needed
	// and within the segment that already being allocated
	uint64_t pageStart = firstFit(size, &found);
	if( found){
		//insert the new allocated page to the
		Bounds allocatedPages = {pageStart,size};
		allocatedPagesInsertion(&allocatedPages);

	}else{
		lockMutex(lastGapStartAddressMutex);
		if((lastGapEndAddress - lastGapStartAddress) >= size) {
//			uint64_t remainingSize = BYTES_2MB - (size % BYTES_2MB);
//			if(remainingSize != 0){
//
//			}
			//creating a new bounds that contains all of the information on the allocation at hand
			Bounds allocatedPages = {lastGapStartAddress,size};
			lastGapStartAddress += size;
			unlockMutex(lastGapStartAddressMutex);

			allocatedPagesInsertion(&allocatedPages);
		}else{
			unlockMutex(lastGapStartAddressMutex);
			return 0;
		}
	}
	return (uint64_t)size;
}

/**
 * insert to an old node if the bounds match
 * @param freePages the pages that are being inserted into the vector
 * @return return true if it was inserted else return false
 */
bool insertionToAnOldNode( Bounds *freePages){
	uint64_t endBound = freePages->start + freePages->size;

	lockMutex(freeMemoryVectorMutex);
	freeVector::iterator iter = freeMemoryVector.begin();

	//search the vector for a match between the bounds
	for(iter;iter != freeMemoryVector.end(); ++iter){
		if((*iter).start == endBound){ //check if the freePages end bound match with the current node start bound
			(*iter).size += freePages->size;
			return true;
		}
		//check if the freePages start bound match with the current node end bound
		else if((*iter).size+ (*iter).start ==freePages->start){
			(*iter).start -= freePages->size;
			return true;
		}
	}
	unlockMutex(freeMemoryVectorMutex);
	return false;
}

/**
 * insert to the freeMemoryVector with respect to the initial and ending bounds of the pages
 * @param freePages the pages being freed
 */
void freePagesInsertion(Bounds *freePages){
	uint64_t endBound = freePages->start+freePages->size;

	//try to insert to an old node
	if (!insertionToAnOldNode( freePages)) {

		lockMutex(lastGapStartAddressMutex);

		if (endBound == lastGapStartAddress) {
			lastGapStartAddress -= freePages->size;
			unlockMutex(lastGapStartAddressMutex);
		} else {
			unlockMutex(lastGapStartAddressMutex);
			//add it as a new node of the vector
			lockMutex(freeMemoryVectorMutex);
			pthread_mutex_lock(freeMemoryVectorMutex);
			freeMemoryVector.push_back(*freePages);
			unlockMutex(freeMemoryVectorMutex);
		}
	}

}

/**
 * free the requested memory
 * @param alloc_id the allocation id
 */
void manager_free(uint64_t alloc_id){
	//search for allocated memory
	lockMutex(allocatedMemoryMapMutex);
	allocMap::iterator search = allocatedMemoryMap.find(alloc_id);
	if(search != allocatedMemoryMap.end()){

		Bounds freePages = search->second;
		allocatedMemoryMap.erase(search);
		unlockMutex(allocatedMemoryMapMutex);
		//add the page the free memory vector
		freePagesInsertion(&freePages);

	}else{
		unlockMutex(allocatedMemoryMapMutex);
	}
}
