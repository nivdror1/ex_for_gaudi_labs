
#include <vector>
#include <map>
#include "MemoryManager.h"
#include <math.h>
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
uint64_t allocCounter;

/** A mutex for the allocation id*/
pthread_mutex_t allocCounterMutex;

/** A map of the allocated memory*/
allocMap allocatedMemoryMap;

/** A mutex for the allocatedMemoryMap*/
pthread_mutex_t allocatedMemoryMapMutex;

/** A vector of the free memory*/
freeVector freeMemoryVector;

/** A mutex of the freeMemoryVector*/
pthread_mutex_t freeMemoryVectorMutex;

/** The end address of the memory sequence*/
uint64_t lastGapEndAddress;

/** the start address of the memory that haven't been allocated yet*/
uint64_t lastGapStartAddress;

/** A mutex for the lastGapStartAddress*/
pthread_mutex_t lastGapStartAddressMutex;


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
 * unlock the mutex
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
	allocCounter = 1;
	lastGapEndAddress = (MAX_INT32 *(uint64_t)(pow(2,4)));
	//initiate the mutexes
	createMutex(&allocatedMemoryMapMutex);
	createMutex(&lastGapStartAddressMutex);
	createMutex(&freeMemoryVectorMutex);
	createMutex(&allocCounterMutex);
}

/**
 * terminate the library
 */
void manager_fini(){
	//destroy mutexes
	destroyMutex(&allocatedMemoryMapMutex);
	destroyMutex(&lastGapStartAddressMutex);
	destroyMutex(&freeMemoryVectorMutex);
	destroyMutex(&allocCounterMutex);

	lastGapEndAddress = lastGapStartAddress = 0;

	allocatedMemoryMap.clear();
	freeMemoryVector.clear();
}

/**
 * insert the allocation information to the allocatedMemoryMap
 * @param allocatedPages a Bounds structure that contain the information of the given allocation
 */
uint64_t allocatedPagesInsertion(struct Bounds *allocatedPages){

	//getting the current allocation id and increase the counter
	lockMutex(&allocCounterMutex);
	uint64_t allocId = allocCounter ;
	allocCounter ++;
	unlockMutex(&allocCounterMutex);

	//insert to the allocatedMemoryMap
	lockMutex(&allocatedMemoryMapMutex);
	allocatedMemoryMap[allocId] = (*allocatedPages);
	unlockMutex(&allocatedMemoryMapMutex);

	return allocId;
}

/**
 * search the freeMemoryVector for a memory hole that fit size that being allocated.
 * @param size the size that the user want to allocate
 * @param found a boolean variable that signify if we found a suitable memory hole
 * @return the start address of allocated memory
 */
uint64_t firstFit(size_t size, bool *found){
	uint64_t startAddress = 0;
	lockMutex(&freeMemoryVectorMutex);

	//search for the a memory allocation that fit the size
	for(unsigned int i = 0; i < freeMemoryVector.size();i++ ){
		//check if the size of the current page is sufficient
		if(freeMemoryVector.at(i).size >= size){
			startAddress = freeMemoryVector.at(i).start;

			//delete the pages from the vector iff there size is zero
			if(freeMemoryVector.at(i).size - size  == 0){
				freeMemoryVector.erase(freeMemoryVector.begin()+i);
			}else{
				freeMemoryVector.at(i).size -= size;
				uint64_t newAddress =startAddress+ size;
				freeMemoryVector.at(i).start = newAddress;
			}
			unlockMutex(&freeMemoryVectorMutex);
			//noted that a suitable allocation has been found
			(*found)=true;
			return startAddress;
		}
	}
	unlockMutex(&freeMemoryVectorMutex);
	return startAddress;
}


/**
 * insert to an old node if the bounds match
 * @param freePages the pages that are being inserted into the vector
 * @return return true if it was inserted else return false
 */
bool insertionToAnOldNode( Bounds *freePages){
    uint64_t endBound = freePages->start + freePages->size;

    lockMutex(&freeMemoryVectorMutex);

    //search the vector for a match between the bounds
    for(unsigned int i = 0 ;i < freeMemoryVector.size(); ++i){
        if(freeMemoryVector.at(i).start == endBound){ //check if the freePages end bound match with the current node start bound
            freeMemoryVector.at(i).size += freePages->size;
            return true;
        }
            //check if the freePages start bound match with the current node end bound
        else if(freeMemoryVector.at(i).size+ freeMemoryVector.at(i).start ==freePages->start){
            freeMemoryVector.at(i).start -= freePages->size;
            return true;
        }
    }
    unlockMutex(&freeMemoryVectorMutex);
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

        lockMutex(&lastGapStartAddressMutex);

        if (endBound == lastGapStartAddress) {
            lastGapStartAddress -= freePages->size;
            unlockMutex(&lastGapStartAddressMutex);
        } else {
            unlockMutex(&lastGapStartAddressMutex);
            //add it as a new node of the vector
            lockMutex(&freeMemoryVectorMutex);
            freeMemoryVector.push_back(*freePages);
            unlockMutex(&freeMemoryVectorMutex);
        }
    }
}

/**
 * allocating the part of the extra page because of the physical allocation restrictions,
 * but since the user won't be using this memory, it is being inserted to the freeMemoryVector
 * @param remainingSize the size to allocate
 * @param extraPageAddress the start address of the allocation
 */
void extraPageAllocation(size_t remainingSize,uint64_t extraPageAddress){
    //adding the remaining allocated memory page to the freeMemoryVector
    lastGapStartAddress += remainingSize;
    unlockMutex(&lastGapStartAddressMutex);
    Bounds remainingAllocatedMemory = {extraPageAddress,remainingSize};

    lockMutex(&freeMemoryVectorMutex);
    freeMemoryVector.push_back(remainingAllocatedMemory);
    unlockMutex(&freeMemoryVectorMutex);
}
/**
 * allocate from the unused memory of the device
 * @param size the size of the allocation that the user wanted
 * @return the allocation id
 */
uint64_t physicalAllocation(size_t size){
    lockMutex(&lastGapStartAddressMutex);
    if((lastGapEndAddress - lastGapStartAddress) >= size) {
        //creating a new bounds that contains all of the information on the allocation at hand
        Bounds allocatedPages = {lastGapStartAddress,size};
        uint64_t extraPageAddress =lastGapStartAddress += size;

        //checking if there is a need to allocate one more 2mb page
        size_t remainingSize = BYTES_2MB - (size % BYTES_2MB);
        if(remainingSize != BYTES_2MB){
            extraPageAllocation(remainingSize, extraPageAddress);

        }else{
            unlockMutex(&lastGapStartAddressMutex);
        }
        return allocatedPagesInsertion(&allocatedPages);
    }else{
        unlockMutex(&lastGapStartAddressMutex);
        return 0;
    }
}

/**
 * manage the allocation
 * @param size the size was the allocation requested
 * @return the size that was allocated
 */
uint64_t manager_malloc(size_t size){
	uint64_t allocId = 0;
	bool found =false;
	if(size <= 0){
		return 0;
	}

	//search for free pages big enough for the allocation needed
	// and within the segment that already being allocated
	uint64_t pageStart = firstFit(size, &found);
	if( found){
		//insert the new allocated page to the allocationMemoryMap
		Bounds allocatedPages = {pageStart,size};

		allocId = allocatedPagesInsertion(&allocatedPages);

	}else{
        //in case there wasn't a memory hole contains in the freeMemoryVector
        //then allocate from the unused memory of the device
		return physicalAllocation(size);
	}
	return allocId;
}

/**
 * free the requested memory
 * @param alloc_id the allocation id
 */
void manager_free(uint64_t alloc_id){
	//search for allocated memory
	lockMutex(&allocatedMemoryMapMutex);
	allocMap::iterator search = allocatedMemoryMap.find(alloc_id);
	if(search != allocatedMemoryMap.end()){

		Bounds freePages = search->second;
		allocatedMemoryMap.erase(search);
		unlockMutex(&allocatedMemoryMapMutex);
		//add the page the free memory vector
		freePagesInsertion(&freePages);

	}else{
		unlockMutex(&allocatedMemoryMapMutex);
	}
}
