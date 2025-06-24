#pragma once

#include <sys/mman.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.c"

#define PAGESIZE      12

#define HUGEPAGES     128

#define STLB_HASHSIZE 8
#define DTLB_HASHSIZE 4
#define STLB_WAYS     12
#define DTLB_WAYS     6
#define STLB_HASHMASK ((1 << STLB_HASHSIZE) - 1)
#define DTLB_HASHMASK ((1 << DTLB_HASHSIZE) - 1)
#define STLB_SET(addr)                                                 \
	(((addr >> PAGESIZE) ^ (addr >> (PAGESIZE + STLB_HASHSIZE))) & \
	    STLB_HASHMASK)
#define DTLB_SET(addr) ((addr >> PAGESIZE) & DTLB_HASHMASK)

#define FLUSH_SET_SIZE (((1UL << (PAGESIZE + STLB_HASHSIZE * 2))) * 2)

uint8_t *flush_set;

inline void
maccess(void *p)
{
	asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax");
}

void
init_tlb_flush(void)
{
	// flush set for 4kB pages
	flush_set = mmap(0, FLUSH_SET_SIZE, PROT_READ, MAP_ANON | MAP_PRIVATE,
	    -1, 0);

	if (flush_set == MAP_FAILED) {
		error("error in tlb flush setup, mmap MAP_FAILED");
	}
}

void
targeted_tlb_flush(size_t addr)
{
	size_t stlb_set = STLB_SET(addr);
	size_t dtlb_set = DTLB_SET(addr);
	size_t flush_base = (size_t)flush_set;
	flush_base = (((flush_base >> (PAGESIZE + STLB_HASHSIZE * 2)))
			 << (PAGESIZE + STLB_HASHSIZE * 2)) +
	    (1UL << (PAGESIZE + STLB_HASHSIZE * 2));

	for (size_t i = 0; i < DTLB_WAYS * 2; i++) {
		size_t evict_addr = (flush_base + (dtlb_set << PAGESIZE)) ^
		    (i << (PAGESIZE + DTLB_HASHSIZE));
		maccess((void *)evict_addr);
	}

	for (size_t i = 0; i < STLB_WAYS * 2; i++) {
		size_t evict_addr = (flush_base + (stlb_set << PAGESIZE)) ^
		    (((i << STLB_HASHSIZE) + i) << PAGESIZE);
		maccess((void *)evict_addr);
	}
}
