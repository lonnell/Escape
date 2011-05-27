/**
 * $Id: kernel.c 863 2010-12-28 11:51:59Z nasmussen $
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

#include <sys/common.h>
#include <sys/arch/eco32/boot.h>
#include <sys/video.h>

/* TODO
static uint8_t initloader[] = {
#if DEBUGGING
#	include "../../build/eco32-debug/user_initloader.dump"
#else
#	include "../../build/eco32-release/user_initloader.dump"
#endif
};
*/

int main(const sLoadProg *progs) {
	size_t i;
	for(i = 0; i < PROG_COUNT; i++) {
		vid_printf("name=%s, start=%08x, size=%08x\n",progs[i].path,progs[i].start,progs[i].size);
	}

	while(1);

#if 0
	sStartupInfo info;
	sThread *t;

	UNUSED(magic);

	/* the first thing we've to do is set up the page-dir and page-table for the kernel and so on
	 * and "correct" the GDT */
	paging_init();
	gdt_init();
	mboot_init(mbp);

	/* init video and serial-ports */
	vid_init();
	ser_init();

	vid_printf("GDT exchanged, paging enabled, video initialized");
	vid_printf("\033[co;2]%|s\033[co]","DONE");

#if DEBUGGING
	mboot_dbg_print();
#endif

	/* mm */
	vid_printf("Initializing physical memory-management...");
	mm_init(mboot_getInfo());
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* paging */
	vid_printf("Initializing paging...");
	paging_mapKernelSpace();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* fpu */
	vid_printf("Initializing FPU...");
	fpu_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* vfs */
	vid_printf("Initializing VFS...");
	vfs_init();
	vfs_info_init();
	vfs_req_init();
	vfs_drv_init();
	vfs_real_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* processes */
	vid_printf("Initializing process-management...");
	ev_init();
	proc_init();
	sched_init();
	/* the process and thread-stuff has to be ready, too ... */
	log_vfsIsReady();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* vmm */
	vid_printf("Initializing virtual memory management...");
	vmm_init();
	cow_init();
	shm_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* idt */
	vid_printf("Initializing IDT...");
	intrpt_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* timer */
	vid_printf("Initializing timer...");
	timer_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* signals */
	vid_printf("Initializing signal-handling...");
	sig_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* cpu */
	vid_printf("Detecting CPU...");
	cpu_detect();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

#if DEBUGGING
	vid_printf("%d free frames (%d KiB)\n",mm_getFreeFrames(MM_CONT | MM_DEF),
			mm_getFreeFrames(MM_CONT | MM_DEF) * PAGE_SIZE / K);
#endif

	/* load initloader */
	if(elf_loadFromMem(initloader,sizeof(initloader),&info) < 0)
		util_panic("Unable to load initloader");
	t = thread_getRunning();
	/* give the process some stack pages */
	t->stackRegion = vmm_add(t->proc,NULL,0,INITIAL_STACK_PAGES * PAGE_SIZE,
			INITIAL_STACK_PAGES * PAGE_SIZE,REG_STACK);
	assert(t->stackRegion >= 0);
	return info.progEntry;
#endif
	return 0;
}
