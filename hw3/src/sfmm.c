/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "debug.h"
#define HEADER_SIZE_BYTES (SF_HEADER_SIZE >> 3)
#define FOOTER_SIZE_BYTES (SF_FOOTER_SIZE >> 3)
#define MIN_PAYLOAD_BYTES (HEADER_SIZE_BYTES + FOOTER_SIZE_BYTES)
#define SPLITER_BYTES (HEADER_SIZE_BYTES + FOOTER_SIZE_BYTES)
#define FLAG_FREE 0
#define FLAG_ALOC 1
#define GET_HEADER(bp) ((uint64_t*)((char*) bp - HEADER_SIZE_BYTES))

static uint64_t sbrk_counter = 0;

/**
 * You should store the heads of your free lists in these variables.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
free_list seg_free_list[4] = {
    {NULL, LIST_1_MIN, LIST_1_MAX},
    {NULL, LIST_2_MIN, LIST_2_MAX},
    {NULL, LIST_3_MIN, LIST_3_MAX},
    {NULL, LIST_4_MIN, LIST_4_MAX}
};

int sf_errno = 0;

static inline uint64_t getListNum(size_t blockSize) {
	if(blockSize <= LIST_1_MAX)
		return 0;
	else {
		if(blockSize <= LIST_2_MAX)
			return 1;
		else {
			if(blockSize <= LIST_3_MAX)
				return 2;
			else
				return 3;
		}
	}
}

static inline size_t getBlockSize(void* ptr) {
	sf_header* header = (sf_header*)ptr;
	return (*header).block_size * MIN_PAYLOAD_BYTES;
}

static inline void* getNextBP(void* bp) {
	uint64_t* hdrp = GET_HEADER(bp);
	size_t shift = getBlockSize(hdrp);
	if((char*)hdrp + shift > (char*)get_heap_end())
		return NULL;
	return (char*)bp + shift;
}

static inline void* getPrevBP(void* bp) {
	if((char*)bp == (char*)get_heap_start() + HEADER_SIZE_BYTES)
		return NULL;
	uint64_t* hdrp = GET_HEADER(bp);
	uint64_t* prevFTRP = (uint64_t*)((char*) hdrp - FOOTER_SIZE_BYTES);
	size_t shift = (*(sf_footer*)prevFTRP).block_size * MIN_PAYLOAD_BYTES;
	return (char*)bp - shift;
}

static void putFits(void* bp) {
	if(bp == NULL || bp < get_heap_start() || bp > get_heap_end()) {
		sf_errno = ENOMEM;
		debug("%s","Here");
		//abort();
		return;
	}
	uint64_t* hdrp = GET_HEADER(bp);
	uint64_t listNum = getListNum(getBlockSize(hdrp));
	sf_free_header* headerNode = (sf_free_header*)hdrp;

	if(seg_free_list[listNum].head != NULL) {
		(*headerNode).next = seg_free_list[listNum].head;
		(*seg_free_list[listNum].head).prev = headerNode;
	}
	else
		(*headerNode).next = NULL;
	seg_free_list[listNum].head = headerNode;
	(*seg_free_list[listNum].head).prev = NULL;

	return;
}

static void delBlkInList(void* bp) {
	if(bp == NULL || bp < get_heap_start() || bp > get_heap_end()) {
		sf_errno = ENOMEM;
		debug("%s","Here");
		//abort();
		return;
	}

	uint64_t* hdrp = GET_HEADER(bp);
	uint64_t listNum = getListNum(getBlockSize(hdrp));
	sf_free_header* header = seg_free_list[listNum].head;
	while(header != NULL) {
		if((uint64_t*)header == hdrp) {
			if((*header).prev == NULL) {
				if((*header).next != NULL) {
					seg_free_list[listNum].head = (*header).next;
					(*header).next = NULL;
					(*seg_free_list[listNum].head).prev = NULL;
				}
				else
					seg_free_list[listNum].head = NULL;
			}
			else {
				if((*header).next != NULL) {
					(*(*header).prev).next = (*header).next;
					(*(*header).next).prev = (*header).prev;
					(*header).next = NULL;
				}
				(*header).prev = NULL;
			}
			return;
		}
		header = (*header).next;
	}
	sf_errno = ENOMEM;
		debug("%s","Here");
	//abort();
	return;
}

static void* findFits(size_t payloadSize) {
	if(payloadSize == 0 || payloadSize == 4 * PAGE_SZ) {
		sf_errno = EINVAL;
		debug("%s","Here");
		//abort();
		return NULL;
	}
	uint64_t listNum = getListNum(payloadSize + HEADER_SIZE_BYTES + FOOTER_SIZE_BYTES);
	for(uint64_t i = listNum; i < FREE_LIST_COUNT; i++) {
		sf_free_header* header = seg_free_list[i].head;
		while(header != NULL) {
			if((*(sf_header*)header).block_size * MIN_PAYLOAD_BYTES >=
					payloadSize + HEADER_SIZE_BYTES + FOOTER_SIZE_BYTES) {
				char* bp = (char*)header + HEADER_SIZE_BYTES;
				delBlkInList(bp);
				return bp;
			}
			header = (*header).next;
		}
	}
	return NULL;
}

static void place(void* bp, size_t payloadSize, size_t requestSize, bool allocFlag) {
	uint64_t* hdrp = GET_HEADER(bp);
	uint64_t* ftrp = (uint64_t*)((char*) bp + payloadSize);
	sf_header header;
	sf_footer footer;

	header.allocated = 0x0; //initialize the bit-field
	footer.allocated = 0x0; //initialize the bit-field

	header.allocated = allocFlag;
	header.padded = (payloadSize != requestSize);
	header.two_zeroes = 0;
	header.block_size = (payloadSize + HEADER_SIZE_BYTES + FOOTER_SIZE_BYTES)
		>> (ALLOCATED_BITS + PADDED_BITS + TWO_ZEROES);

	footer.allocated = allocFlag;
	footer.padded = (payloadSize != requestSize);
	footer.two_zeroes = 0;
	footer.block_size = (payloadSize + HEADER_SIZE_BYTES + FOOTER_SIZE_BYTES)
		>> (ALLOCATED_BITS + PADDED_BITS + TWO_ZEROES);
	footer.requested_size = requestSize;

	*hdrp = *(uint64_t*)&header;
	*ftrp = *(uint64_t*)&footer;
	return;
}

static void* coalesce(void* bp, void* prevBP, void* nextBP) {
	size_t blockSize, payloadSize;
	if(prevBP == NULL) {
		if(nextBP == NULL) {
			return bp;
		}
		//case of free
		blockSize = getBlockSize(GET_HEADER(bp)) + getBlockSize(GET_HEADER(nextBP));
		payloadSize = blockSize - HEADER_SIZE_BYTES - FOOTER_SIZE_BYTES;
		place(bp, payloadSize, payloadSize, FLAG_FREE);
		return bp;
	}
	else {
		if(nextBP == NULL) {
			//case of extendHeap and sbrk
			blockSize = getBlockSize(GET_HEADER(prevBP)) + getBlockSize(GET_HEADER(bp));
			payloadSize = blockSize - HEADER_SIZE_BYTES - FOOTER_SIZE_BYTES;
			place(prevBP, payloadSize, payloadSize, FLAG_FREE);
			return prevBP;
		}
		else {
			//No such case in HW3
			blockSize = getBlockSize(GET_HEADER(prevBP)) + getBlockSize(GET_HEADER(bp)) + getBlockSize(GET_HEADER(nextBP));
			payloadSize = blockSize - HEADER_SIZE_BYTES - FOOTER_SIZE_BYTES;
			place(prevBP, payloadSize, payloadSize, FLAG_FREE);
			return prevBP;
		}
	}
}

static void* extendHeap(size_t size) {
	if(sbrk_counter == 4) {
		sf_errno = ENOMEM;
		debug("%s","Here");
		//abort();
		return NULL;
	}
	char *bp;
	size_t blkSize = size + SPLITER_BYTES;
	uint64_t pageNum = (blkSize > PAGE_SZ) ? (blkSize / PAGE_SZ) + 1 : 1;
	for(int i = 1; i <= pageNum; i++) {
		sbrk_counter++;
		if(i == 1)
			bp = sf_sbrk() + HEADER_SIZE_BYTES;
		else
			sf_sbrk();
	}
	place(bp, pageNum * PAGE_SZ - SPLITER_BYTES, pageNum * PAGE_SZ - SPLITER_BYTES, FLAG_FREE);

	char* prevBP = getPrevBP(bp);
	if(prevBP != NULL && (*(sf_header*)GET_HEADER(prevBP)).allocated != 1) {
		delBlkInList(prevBP);
		bp = coalesce(bp, prevBP, NULL);
	}
	return bp;
}

static void splitBlock(void* bp, size_t freeBlkSize) {
	size_t ocpyBlkSize = getBlockSize(GET_HEADER(bp));
	char* nextBP = getNextBP(bp);
	place(nextBP, freeBlkSize - ocpyBlkSize - HEADER_SIZE_BYTES - FOOTER_SIZE_BYTES,
			freeBlkSize - ocpyBlkSize - HEADER_SIZE_BYTES - FOOTER_SIZE_BYTES,
			FLAG_FREE);
	putFits(nextBP);
	return; 
}

void *sf_malloc(size_t size) {
	if(size == 0) {
		sf_errno = EINVAL;
		debug("%s","Here");
		abort();
	}
	size_t payloadSize = (1 + ((size - 1) / MIN_PAYLOAD_BYTES)) * MIN_PAYLOAD_BYTES; //Ceiling
	char* bp;
	if(payloadSize >= 4 * PAGE_SZ) {
		sf_errno = ENOMEM;
		debug("%s","Here");
		return NULL;
	}
	if((bp = findFits(payloadSize)) == NULL) {
		if((bp = extendHeap(size)) == NULL) {
			sf_errno = ENOMEM;
		debug("%s","Here");
			return NULL;
		}
	}
	size_t freeBlkSize = getBlockSize(GET_HEADER(bp));
	if(freeBlkSize > payloadSize + HEADER_SIZE_BYTES + FOOTER_SIZE_BYTES + SPLITER_BYTES) {
		place(bp, payloadSize, size, FLAG_ALOC);
		splitBlock(bp, freeBlkSize);
	}
	else
		place(bp, freeBlkSize - HEADER_SIZE_BYTES - FOOTER_SIZE_BYTES, size, FLAG_ALOC);
	return bp;
}

void *sf_realloc(void *ptr, size_t size) {
	if(size == 0) {
		sf_free(ptr);
		return NULL;
	}
	if(ptr == NULL || ptr < get_heap_start() || ptr > get_heap_end())
		sf_errno = ENOMEM;
	uint64_t* hdrp = GET_HEADER(ptr);
	size_t blkSize = getBlockSize(hdrp);
	size_t payloadSize = blkSize - HEADER_SIZE_BYTES - FOOTER_SIZE_BYTES;
	uint64_t* ftrp = (uint64_t*)((char*) hdrp + blkSize - FOOTER_SIZE_BYTES);
	size_t reqSize = (*(sf_footer*)ftrp).requested_size;
	sf_header* curHeader = (sf_header*) hdrp;
	if(((*curHeader).allocated != 1) ||
		((*curHeader).padded == 1 && reqSize == payloadSize) ||
		((*curHeader).padded == 0 && reqSize != payloadSize)) {
		sf_errno = EINVAL;
	}
	if(sf_errno != 0)
		abort();
	if(size == reqSize)
		return ptr;
	else if(size > reqSize) {
		char *bp = sf_malloc(size);
		if(bp != NULL)
			memcpy(bp, ptr, reqSize);
		sf_free(ptr);
		return bp;
	}
	else {
		size_t newPayloadSize = (1 + ((size - 1) / MIN_PAYLOAD_BYTES)) * MIN_PAYLOAD_BYTES; //Ceiling
		if(payloadSize - newPayloadSize > SPLITER_BYTES) {
			size_t newBlkSize = newPayloadSize + SPLITER_BYTES;
			place(ptr, newPayloadSize, size, FLAG_ALOC);
			char* nextBP = getNextBP(ptr);
			place(nextBP, blkSize - newBlkSize - SPLITER_BYTES, blkSize - newBlkSize - SPLITER_BYTES, FLAG_ALOC);
			sf_free(nextBP);
		}
		else {
			place(ptr, payloadSize, size, FLAG_ALOC);
		}
		return ptr;
	}
}

void sf_free(void *ptr) {
	if(ptr == NULL || ptr < get_heap_start() || ptr > get_heap_end()){
		debug("%s","Here");
		sf_errno = ENOMEM;
	}
	uint64_t* hdrp = GET_HEADER(ptr);
	size_t blkSize = getBlockSize(hdrp);
	size_t payloadSize = blkSize - HEADER_SIZE_BYTES - FOOTER_SIZE_BYTES;
	uint64_t* ftrp = (uint64_t*)((char*) hdrp + blkSize - FOOTER_SIZE_BYTES);
	size_t reqSize = (*(sf_footer*)ftrp).requested_size;
	sf_header* curHeader = (sf_header*) hdrp;
	if(((*curHeader).allocated != 1) ||
		((*curHeader).padded == 1 && reqSize == payloadSize) ||
		((*curHeader).padded == 0 && reqSize != payloadSize)) {
		debug("%s","Here");
		sf_errno = EINVAL;
	}
	if(sf_errno != 0){
		debug("%s","Here");
		abort();}

	(*curHeader).allocated = 0;
	(*(sf_footer*)ftrp).allocated = 0;
	
	uint64_t* nextBP = getNextBP(ptr);
	if(nextBP != NULL)
	{
		sf_header* nextHeader = (sf_header*)GET_HEADER(nextBP);
		if((*nextHeader).allocated != 1) {
			delBlkInList(nextBP);
			coalesce(ptr, NULL, nextBP);
		}
	}
	
	putFits(ptr);
	return;
}
