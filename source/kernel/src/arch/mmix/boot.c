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
#include <sys/mem/paging.h>
#include <sys/mem/cache.h>
#include <sys/mem/swap.h>
#include <sys/mem/vmm.h>
#include <sys/mem/cow.h>
#include <sys/mem/sharedmem.h>
#include <sys/mem/dynarray.h>
#include <sys/arch/mmix/mem/addrspace.h>
#include <sys/task/proc.h>
#include <sys/task/thread.h>
#include <sys/task/event.h>
#include <sys/task/sched.h>
#include <sys/task/elf.h>
#include <sys/task/uenv.h>
#include <sys/task/timer.h>
#include <sys/task/smp.h>
#include <sys/vfs/node.h>
#include <sys/vfs/vfs.h>
#include <sys/vfs/channel.h>
#include <sys/vfs/request.h>
#include <sys/vfs/driver.h>
#include <sys/vfs/real.h>
#include <sys/vfs/info.h>
#include <sys/vfs/info.h>
#include <sys/log.h>
#include <sys/config.h>
#include <sys/boot.h>
#include <sys/util.h>
#include <sys/video.h>
#include <string.h>

#define MAX_ARG_COUNT			8
#define MAX_ARG_LEN				64

static const char **boot_parseArgs(const char *line,int *argc);

static sLoadProg progs[MAX_PROG_COUNT];
static sBootInfo info;
static int bootState = 0;
static int bootFinished = 1;

void boot_init(const sBootInfo *binfo,bool logToVFS) {
	int argc;
	const char **argv;

	/* make a copy of the bootinfo, since the location it is currently stored in will be overwritten
	 * shortly */
	memcpy(&info,binfo,sizeof(sBootInfo));
	info.progs = progs;
	memcpy((void*)info.progs,binfo->progs,sizeof(sLoadProg) * binfo->progCount);
	/* start idle-thread, load programs (without kernel) and wait for programs */
	bootFinished = (info.progCount - 1) * 2 + 1;

	vid_init();

	/* parse the boot parameter */
	argv = boot_parseArgs(binfo->progs[0].command,&argc);
	conf_parseBootParams(argc,argv);

#if DEBUGGING
	boot_print();
#endif

	/* mm */
	vid_printf("Initializing physical memory-management...");
	pmem_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* paging */
	vid_printf("Initializing paging...");
	aspace_init();
	paging_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* smp */
	vid_printf("Initializing SMP...");
	dyna_init();
	smp_init();
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
	if(logToVFS)
		log_vfsIsReady();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* vmm */
	vid_printf("Initializing virtual memory management...");
	vmm_init();
	cow_init();
	shm_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* timer */
	vid_printf("Initializing timer...");
	timer_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

	/* signals */
	vid_printf("Initializing signal-handling...");
	sig_init();
	vid_printf("\033[co;2]%|s\033[co]","DONE");

#if DEBUGGING
	vid_printf("%d free frames (%d KiB)\n",pmem_getFreeFrames(MM_CONT | MM_DEF),
			pmem_getFreeFrames(MM_CONT | MM_DEF) * PAGE_SIZE / K);
#endif
}

const sBootInfo *boot_getInfo(void) {
	return &info;
}

size_t boot_getKernelSize(void) {
	return progs[0].size;
}

size_t boot_getModuleSize(void) {
	uintptr_t start = progs[1].start;
	uintptr_t end = progs[info.progCount - 1].start + progs[info.progCount - 1].size;
	return end - start;
}

size_t boot_getUsableMemCount(void) {
	return info.memSize;
}

int boot_loadModules(sIntrptStackFrame *stack) {
	UNUSED(stack);
	size_t i;
	pid_t pid;

	/* it's not good to do this twice.. */
	if(bootState == bootFinished)
		return 0;

	if(bootState == 0) {
		/* start idle-thread */
		if(proc_startThread(0,T_IDLE,NULL) == thread_getRunning()->tid) {
			thread_idle();
			util_panic("Idle returned");
		}
		bootState++;
	}
	else if((bootState % 2) == 1) {
		i = (bootState / 2) + 1;
		/* clone proc */
		pid = proc_getFreePid();
		if(pid == INVALID_PID)
			util_panic("No free process-slots");

		if(proc_clone(pid,0)) {
			sStartupInfo sinfo;
			size_t argSize = 0;
			char *argBuffer = NULL;
			sProc *p = proc_getRunning();
			/* parse args */
			int argc;
			const char **argv = boot_parseArgs(progs[i].command,&argc);
			if(argc < 2)
				util_panic("Invalid arguments for boot-module: %s\n",progs[i].command);
			/* remove regions (except stack) */
			proc_removeRegions(p,false);
			/* now load module */
			proc_setCommand(p,argv[0]);
			memcpy((char*)p->command,argv[0],strlen(argv[0]) + 1);
			if(elf_loadFromMem((const void*)progs[i].start,progs[i].size,&sinfo) < 0)
				util_panic("Loading boot-module %s failed",p->command);
			/* build args */
			argc = proc_buildArgs(argv,&argBuffer,&argSize,false);
			if(argc < 0)
				util_panic("Building args for boot-module %s failed: %d",p->command,argc);
			/* no dynamic linking here */
			p->entryPoint = sinfo.progEntry;
			thread_addHeapAlloc(argBuffer);
			if(!uenv_setupProc(p->command,argc,argBuffer,argSize,&sinfo,sinfo.progEntry))
				util_panic("Unable to setup user-stack for boot module %s",p->command);
			thread_remHeapAlloc(argBuffer);
			cache_free(argBuffer);
			/* we don't want to continue ;) */
			return 0;
		}

		bootState++;
	}
	else {
		i = bootState / 2;
		inode_t nodeNo;
		int argc;
		const char **argv = boot_parseArgs(progs[i].command,&argc);

		/* wait until the driver is registered */
		vid_printf("Waiting for '%s'...\n",argv[0]);
		/* don't create a pipe- or driver-usage-node here */
		while(vfs_node_resolvePath(argv[1],&nodeNo,NULL,VFS_NOACCESS) < 0) {
			/* Note that we HAVE TO sleep here because we may be waiting for ata and fs is not
			 * started yet. I.e. if ata calls sleep() there is no other runnable thread (except
			 * idle, but its just chosen if nobody else wants to run), so that we wouldn't make
			 * a switch but stay here for ever (and get no timer-interrupts to wakeup ata) */
			timer_sleepFor(thread_getRunning()->tid,20);
			thread_switch();
		}

		bootState++;
	}

	/* TODO */
#if 0
	/* start the swapper-thread. it will never return */
	if(proc_startThread(0,0,NULL) == thread_getRunning()->tid) {
		swap_start();
		util_panic("Swapper reached this");
	}
#endif

	if(bootState == bootFinished) {
		/* if not requested otherwise, from now on, print only to log */
		if(!conf_get(CONF_LOG2SCR))
			vid_setTargets(TARGET_LOG);
	}
	return bootState == bootFinished ? 0 : 1;
}

void boot_print(void) {
	size_t i;
	vid_printf("Memory size: %zu bytes\n",info.memSize);
	vid_printf("Disk size: %zu bytes\n",info.diskSize);
	vid_printf("Kernelstack-begin: %p\n",info.kstackBegin);
	vid_printf("Kernelstack-end: %p\n",info.kstackEnd);
	vid_printf("Boot modules:\n");
	/* skip kernel */
	for(i = 0; i < info.progCount; i++) {
		vid_printf("\t%s\n\t\t[%p .. %p]\n",info.progs[i].command,
				info.progs[i].start,info.progs[i].start + info.progs[i].size);
	}
}

static const char **boot_parseArgs(const char *line,int *argc) {
	static char argvals[MAX_ARG_COUNT][MAX_ARG_LEN];
	static char *args[MAX_ARG_COUNT];
	size_t i = 0,j = 0;
	args[0] = argvals[0];
	while(*line) {
		if(*line == ' ') {
			if(args[j][0]) {
				if(j + 1 >= MAX_ARG_COUNT)
					break;
				args[j][i] = '\0';
				j++;
				i = 0;
				args[j] = argvals[j];
			}
		}
		else if(i < MAX_ARG_LEN)
			args[j][i++] = *line;
		line++;
	}
	*argc = j + 1;
	args[j][i] = '\0';
	return (const char**)args;
}
