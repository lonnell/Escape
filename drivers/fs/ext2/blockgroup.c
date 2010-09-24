/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
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

#include <esc/common.h>
#include <esc/debug.h>
#include <esc/thread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ext2.h"
#include "../blockcache.h"
#include "blockgroup.h"
#include "rw.h"

bool ext2_bg_init(sExt2 *e) {
	/* read block-group-descriptors */
	u32 bcount = EXT2_BYTES_TO_BLKS(e,ext2_getBlockGroupCount(e));
	e->groupsDirty = false;
	e->groups = (sExt2BlockGrp*)malloc(bcount * EXT2_BLK_SIZE(e));
	if(e->groups == NULL) {
		printe("Unable to allocate memory for blockgroups");
		return false;
	}
	if(!ext2_rw_readBlocks(e,e->groups,e->superBlock.firstDataBlock + 1,bcount)) {
		free(e->groups);
		printe("Unable to read group-table");
		return false;
	}
	return true;
}

void ext2_bg_destroy(sExt2 *e) {
	free(e->groups);
}

void ext2_bg_update(sExt2 *e) {
	u32 i,bno,count,bcount;
	assert(lock(EXT2_SUPERBLOCK_LOCK,LOCK_EXCLUSIVE | LOCK_KEEP) == 0);

	if(!e->groupsDirty)
		goto done;
	bcount = EXT2_BYTES_TO_BLKS(e,ext2_getBlockGroupCount(e));

	/* update main block-group-descriptor-table */
	if(!ext2_rw_writeBlocks(e,e->groups,e->superBlock.firstDataBlock + 1,bcount)) {
		printe("Unable to update block-group-descriptor-table in blockgroup 0");
		goto done;
	}

	/* update block-group-descriptor backups */
	bno = e->superBlock.blocksPerGroup + EXT2_BLOGRPTBL_BNO;
	count = ext2_getBlockGroupCount(e);
	for(i = 1; i < count; i++) {
		if(ext2_bgHasBackups(e,i)) {
			if(!ext2_rw_writeBlocks(e,e->groups,bno,bcount)) {
				printe("Unable to update block-group-descriptor-table in blockgroup %d",i);
				goto done;
			}
		}
		bno += e->superBlock.blocksPerGroup;
	}

	/* now we're in sync */
	e->groupsDirty = false;
done:
	assert(unlock(EXT2_SUPERBLOCK_LOCK) == 0);
}

#if DEBUGGING

/**
 * Prints the given bitmap
 */
static void ext2_bg_printRanges(sExt2 *e,const char *name,u32 first,u32 max,u8 *bitmap);

void ext2_bg_print(sExt2 *e,u32 no,sExt2BlockGrp *bg) {
	sCBlock *bbitmap = bcache_request(&e->blockCache,bg->blockBitmap,BMODE_READ);
	sCBlock *ibitmap = bcache_request(&e->blockCache,bg->inodeBitmap,BMODE_READ);
	if(!bbitmap || !ibitmap)
		return;
	printf("	blockBitmapStart = %d\n",bg->blockBitmap);
	printf("	inodeBitmapStart = %d\n",bg->inodeBitmap);
	printf("	inodeTableStart = %d\n",bg->inodeTable);
	printf("	freeBlocks = %d\n",bg->freeBlockCount);
	printf("	freeInodes = %d\n",bg->freeInodeCount);
	printf("	usedDirCount = %d\n",bg->usedDirCount);
	ext2_bg_printRanges(e,"Blocks",no * e->superBlock.blocksPerGroup,
			MIN(e->superBlock.blocksPerGroup,
			e->superBlock.blockCount - (no * e->superBlock.blocksPerGroup)),bbitmap->buffer);
	ext2_bg_printRanges(e,"Inodes",no * e->superBlock.inodesPerGroup,
			MIN(e->superBlock.inodesPerGroup,
			e->superBlock.inodeCount - (no * e->superBlock.inodesPerGroup)),ibitmap->buffer);
	bcache_release(ibitmap);
	bcache_release(bbitmap);
}

static void ext2_bg_printRanges(sExt2 *e,const char *name,u32 first,u32 max,u8 *bitmap) {
	bool lastFree;
	u32 pcount,start,i,a,j;

	pcount = 0;
	start = 0;
	lastFree = (bitmap[0] & 1) == 0;
	printf("	Free%s:\n\t\t",name);
	for(i = 0; i < (u32)EXT2_BLK_SIZE(e); a = 0, i++) {
		for(a = 0, j = 1; j < 256; a++, j <<= 1) {
			if(i * 8 + a >= max)
				goto freeDone;
			if(bitmap[i] & j) {
				if(lastFree) {
					if(start < i * 8 + a) {
						printf("%d .. %d, ",first + start,first + i * 8 + a - 1);
						if(++pcount % 4 == 0)
							printf("\n\t\t");
					}
					start = i * 8 + a;
					lastFree = false;
				}
			}
			else if(!lastFree) {
				start = i * 8 + a;
				lastFree = true;
			}
		}
	}
freeDone:
	if(lastFree && start < i * 8 + a)
		printf("%d .. %d, ",first + start,first + i * 8 + a - 1);
	printf("\n");

	pcount = 0;
	start = 0;
	lastFree = (bitmap[0] & 1) == 0;
	printf("	Used%s:\n\t\t",name);
	for(i = 0; i < (u32)EXT2_BLK_SIZE(e); a = 0, i++) {
		for(a = 0, j = 1; j < 256; a++, j <<= 1) {
			if(i * 8 + a >= max)
				goto usedDone;
			if((bitmap[i] & j)) {
				if(lastFree) {
					start = i * 8 + a;
					lastFree = false;
				}
			}
			else if(!lastFree) {
				if(start < i * 8 + a) {
					printf("%d .. %d, ",first + start,first + i * 8 + a - 1);
					if(++pcount % 4 == 0)
						printf("\n\t\t");
				}
				start = i * 8 + a;
				lastFree = true;
			}
		}
	}
usedDone:
	if(!lastFree && start < i * 8 + a)
		printf("%d .. %d, ",first + start,first + i * 8 + a - 1);
	printf("\n");
}

#endif
