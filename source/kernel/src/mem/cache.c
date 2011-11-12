/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <sys/common.h>
#include <sys/mem/cache.h>
#include <sys/mem/paging.h>
#include <sys/mem/kheap.h>
#include <sys/spinlock.h>
#include <sys/log.h>
#include <sys/video.h>
#include <string.h>

#if DEBUGGING
#define DEBUG_ALLOC_N_FREE	0
#endif

#define GUARD_MAGIC			0xCAFEBABE
#define MIN_OBJ_COUNT		8
#define SIZE_THRESHOLD		128
#define HEAP_THRESHOLD		512

typedef struct {
	const size_t objSize;
	size_t totalObjs;
	size_t freeObjs;
	void *freeList;
} sCache;

static void *cache_get(sCache *c,size_t i);

static klock_t cacheLock;
static size_t pages = 0;
static sCache caches[] = {
	{8,0,0,NULL},
	{16,0,0,NULL},
	{32,0,0,NULL},
	{64,0,0,NULL},
	{128,0,0,NULL},
	{256,0,0,NULL},
	{384,0,0,NULL},	/* e.g. for processes */
	{512,0,0,NULL},
	{1024,0,0,NULL},
	{2048,0,0,NULL},
	{4096,0,0,NULL},
	{8192,0,0,NULL},
	{16384,0,0,NULL},
};

#if DEBUGGING
static bool aafEnabled = false;
#endif

void *cache_alloc(size_t size) {
	size_t i;
	void *res;
	if(size == 0)
		return NULL;

	for(i = 0; i < ARRAY_SIZE(caches); i++) {
		size_t objSize = caches[i].objSize;
		if(objSize >= size) {
			if((objSize - size) >= SIZE_THRESHOLD)
				break;
			res = cache_get(caches + i,i);
			goto done;
		}
	}
	res = kheap_alloc(size);

done:
#if DEBUG_ALLOC_N_FREE
	if(aafEnabled) {
		spinlock_aquire(&cacheLock);
		util_printEventTrace(util_getKernelStackTrace(),"\n[A] %Px %zu ",res,size);
		spinlock_release(&cacheLock);
	}
#endif
	return res;
}

void *cache_calloc(size_t num,size_t size) {
	void *p = cache_alloc(num * size);
	if(p)
		memclear(p,num * size);
	return p;
}

void *cache_realloc(void *p,size_t size) {
	ulong *area;
	size_t objSize;
	void *res;
	if(p == NULL)
		return cache_alloc(size);

	area = (ulong*)p - 2;
	/* if the guard is not ours, perhaps it has been allocated on the fallback-heap */
	if(area[1] != GUARD_MAGIC)
		return kheap_realloc(p,size);

	assert(area[0] < ARRAY_SIZE(caches));
	objSize = caches[area[0]].objSize;
	if(objSize >= size)
		return p;
	res = cache_alloc(size);
	if(res) {
		memcpy(res,p,objSize);
		cache_free(p);
	}
	return res;
}

void cache_free(void *p) {
	sCache *c;
	ulong *area = (ulong*)p - 2;
	size_t objSize;
	if(p == NULL)
		return;

#if DEBUG_ALLOC_N_FREE
	if(aafEnabled) {
		spinlock_aquire(&cacheLock);
		util_printEventTrace(util_getKernelStackTrace(),"\n[F] %Px 0 ",p);
		spinlock_release(&cacheLock);
	}
#endif

	/* if the guard is not ours, perhaps it has been allocated on the fallback-heap */
	if(area[1] != GUARD_MAGIC) {
		kheap_free(p);
		return;
	}

	/* check whether objSize is within the existing sizes */
	assert(area[0] < ARRAY_SIZE(caches));
	objSize = caches[area[0]].objSize;
	assert(objSize >= caches[0].objSize && objSize <= caches[ARRAY_SIZE(caches) - 1].objSize);
	/* check guard */
	assert(area[(objSize / sizeof(ulong)) + 2] == GUARD_MAGIC);

	/* put on freelist */
	c = caches + area[0];
	spinlock_aquire(&cacheLock);
	area[0] = (ulong)c->freeList;
	c->freeList = area;
	c->freeObjs++;
	spinlock_release(&cacheLock);
}

size_t cache_getPageCount(void) {
	return pages;
}

size_t cache_getOccMem(void) {
	size_t i,count = 0;
	for(i = 0; i < ARRAY_SIZE(caches); i++)
		count += BYTES_2_PAGES(caches[i].totalObjs * (caches[i].objSize + sizeof(ulong) * 3));
	return count * PAGE_SIZE;
}

size_t cache_getUsedMem(void) {
	size_t i,count = 0;
	for(i = 0; i < ARRAY_SIZE(caches); i++)
		count += (caches[i].totalObjs - caches[i].freeObjs) * (caches[i].objSize + sizeof(ulong) * 3);
	return count;
}

void cache_print(void) {
	size_t i;
	for(i = 0; i < ARRAY_SIZE(caches); i++) {
		vid_printf("Cache %zu [size=%zu, total=%zu, free=%zu, pages=%zu]:\n",i,caches[i].objSize,
				caches[i].totalObjs,caches[i].freeObjs,
				BYTES_2_PAGES(caches[i].totalObjs * (caches[i].objSize + sizeof(ulong) * 3)));
	}
}

static void *cache_get(sCache *c,size_t i) {
	ulong *area;
	spinlock_aquire(&cacheLock);
	if(!c->freeList) {
		size_t pageCount = BYTES_2_PAGES(MIN_OBJ_COUNT * c->objSize);
		size_t bytes = pageCount * PAGE_SIZE;
		size_t totalObjSize = c->objSize + sizeof(ulong) * 3;
		size_t j,objs = bytes / totalObjSize;
		size_t rem = bytes - objs * totalObjSize;
		ulong *space = (ulong*)kheap_allocSpace(pageCount);
		if(space == NULL) {
			spinlock_release(&cacheLock);
			return NULL;
		}

		pages += pageCount;
		/* if the remaining space is big enough (it won't bring advantages to add dozens e.g. 8
		 * byte large areas to the heap), add it to the fallback-heap */
		if(rem >= HEAP_THRESHOLD)
			kheap_addMemory((uintptr_t)space + bytes - rem,rem);

		c->totalObjs += objs;
		c->freeObjs += objs;
		for(j = 0; j < objs; j++) {
			space[0] = (ulong)c->freeList;
			c->freeList = space;
			space += (c->objSize / sizeof(ulong)) + 3;
		}
	}

	/* get first from freelist */
	area = (ulong*)c->freeList;
	c->freeList = (void*)area[0];

	/* store size and put guards in front and behind the area */
	area[0] = i;
	area[1] = GUARD_MAGIC;
	area[(c->objSize / sizeof(ulong)) + 2] = GUARD_MAGIC;
	c->freeObjs--;
	spinlock_release(&cacheLock);
	return area + 2;
}

#if DEBUGGING

void cache_dbg_setAaFEnabled(bool enabled) {
	aafEnabled = enabled;
}

#endif
