/**
 * $Id: uenv.c 900 2011-06-02 20:18:17Z nasmussen $
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
#include <sys/task/uenv.h>
#include <sys/task/thread.h>
#include <sys/mem/vmm.h>
#include <sys/mem/paging.h>
#include <sys/vfs/vfs.h>
#include <sys/vfs/real.h>
#include <sys/boot.h>
#include <sys/log.h>
#include <sys/cpu.h>
#include <string.h>
#include <errors.h>
#include <assert.h>

#define KEYBOARD_BASE		0x8006000000000000
#define KEYBOARD_CTRL		0
#define KEYBOARD_IEN		0x02

/* storage for "delayed" signal handling */
typedef struct {
	uint8_t active;
	tTid tid;
	tSig sig;
} sSignalData;

static void uenv_addArgs(sThread *t,const sStartupInfo *info,uint64_t *rsp,uint64_t *ssp,
		uintptr_t entry,uintptr_t tentry,bool thread);

/* temporary storage for signal-handling */
static sSignalData signalData;

void uenv_handleSignal(void) {
	tTid tid;
	tSig sig;
	/* don't do anything, if we should already handle a signal */
	if(signalData.active == 1)
		return;

	if(sig_hasSignal(&sig,&tid)) {
		signalData.active = 1;
		signalData.sig = sig;
		signalData.tid = tid;

		/* a small trick: we store the signal to handle and manipulate the user-stack
		 * and so on later. if the thread is currently running everything is fine. we return
		 * from here and intrpt_handleSignalFinish() will be called.
		 * if the target-thread is not running we switch to him now. the thread is somewhere
		 * in the kernel but he will leave the kernel IN EVERY CASE at the end of the interrupt-
		 * handler. So if we do the signal-stuff at the end we'll get there and will
		 * manipulate the user-stack.
		 * This is simpler than mapping the user-stack and kernel-stack of the other thread
		 * in the current page-dir and so on :)
		 */
		if(thread_getRunning()->tid != tid) {
			/* ensure that the thread is ready */
			/* this may fail because perhaps we're involved in a swapping-operation or similar */
			/* in this case do nothing, we'll handle the signal later (handleSignalFinish() cares
			 * about that) */
			if(thread_setReady(tid))
				thread_switchTo(tid);
		}
	}
}

bool uenv_hasSignalToStart(void) {
	return signalData.active;
}

void uenv_startSignalHandler(sIntrptStackFrame *stack) {
	UNUSED(stack);
	sThread *t = thread_getRunning();
	fSignal handler;
	uint64_t *sp = (uint64_t*)(t->kstackEnd[-15]);	/* $254 */
	/* rBB,rWW,rXX,rYY,rZZ + rJ + signal + handler */
	const ulong count = 5 + 1 + 1 + 1;

	/* release signal-data */
	signalData.active = 0;

	/* if the thread_switchTo() wasn't successfull it means that we have tried to switch to
	 * multiple threads during idle. So we ignore it and try to give the signal later to the thread */
	if(t->tid != signalData.tid)
		return;

	/* extend the stack, if necessary */
	if(thread_extendStack((uintptr_t)(sp - count)) < 0) {
		proc_segFault(t->proc);
		return;
	}
	/* will handle copy-on-write */
	paging_isRangeUserWritable((uintptr_t)(sp - count),count * sizeof(uint64_t));

	/* thread_extendStack() and paging_isRangeUserWritable() may cause a thread-switch. therefore
	 * we may have delivered another signal in the meanwhile... */
	if(t->tid != signalData.tid)
		return;

	handler = sig_startHandling(signalData.tid,signalData.sig);

	/* backup rBB, rWW, rXX, rYY and rZZ */
	cpu_getKSpecials(sp - 1,sp - 2,sp - 3,sp - 4,sp - 5);
	sp -= 6;
	*sp-- = t->kstackEnd[-9];			/* rJ */
	*sp-- = (uintptr_t)handler;
	*sp = signalData.sig;
	t->kstackEnd[-15] = (uint64_t)sp;	/* $254 */

	/* jump to sigRetAddr for setup and finish-code */
	cpu_setKSpecials(0,t->proc->sigRetAddr,1UL << 63,0,0);
}

int uenv_finishSignalHandler(sIntrptStackFrame *stack,tSig signal) {
	UNUSED(stack);
	sThread *t = thread_getRunning();
	uint64_t *regs;
	uint64_t *sp = (uint64_t*)(t->kstackEnd[-15]);	/* $254 */
	/* rBB,rWW,rXX,rYY,rZZ + rJ + signal + handler */
	const ulong count = 5 + 1 + 1 + 1;
	if(!paging_isRangeUserReadable((uintptr_t)sp,count * sizeof(uint64_t)))
		return ERR_INVALID_ARGS;

	/* restore rBB, rWW, rXX, rYY and rZZ */
	sp += 2;
	t->kstackEnd[-9] = *sp++;			/* rJ */
	cpu_setKSpecials(sp[4],sp[3],sp[2],sp[1],sp[0]);
	sp += 5;
	t->kstackEnd[-15] = (uint64_t)sp;	/* $254 */

	/* reenable device-interrupts */
	switch(signal) {
		case SIG_INTRPT_KB:
			regs = (uint64_t*)KEYBOARD_BASE;
			regs[KEYBOARD_CTRL] |= KEYBOARD_IEN;
			break;
		/* not necessary for disk here; the driver will reenable interrupts as soon as a new
		 * command is started */
	}
	return 0;
}

bool uenv_setupProc(const char *path,int argc,const char *args,size_t argsSize,
		const sStartupInfo *info,uintptr_t entryPoint) {
	UNUSED(path);
	uint64_t *ssp,*rsp;
	char **argv;
	size_t totalSize;
	sThread *t = thread_getRunning();

	/*
	 * Initial software stack:
	 * +------------------+  <- top
	 * |     arguments    |
	 * |        ...       |
	 * +------------------+
	 * |     stack-end    |  used for UNSAVE
	 * +------------------+
	 *
	 * Registers:
	 * $1 = argc
	 * $2 = argv
	 * $4 = entryPoint (0 for initial thread, thread-entrypoint for others)
	 * $5 = TLSStart (0 if not present)
	 * $6 = TLSSize (0 if not present)
	 */

	/* we need to know the total number of bytes we'll store on the stack */
	totalSize = sizeof(void*);
	if(argc > 0) {
		/* first round the size of the arguments up. then we need argc+1 pointer */
		totalSize += (argsSize + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1);
		totalSize += sizeof(void*) * (argc + 1);
	}

	/* get register-stack */
	vmm_getRegRange(t->proc,t->stackRegions[0],(uintptr_t*)&rsp,NULL);
	/* get software-stack */
	vmm_getRegRange(t->proc,t->stackRegions[1],NULL,(uintptr_t*)&ssp);

	/* extend the stack if necessary */
	if(thread_extendStack((uintptr_t)ssp - totalSize) < 0)
		return false;
	/* will handle copy-on-write */
	paging_isRangeUserWritable((uintptr_t)ssp - totalSize,totalSize);
	/* register stack is already writeable (done by ELF-loader) */

	/* copy arguments on the user-stack (8byte space) */
	ssp--;
	argv = NULL;
	if(argc > 0) {
		char *str;
		int i;
		size_t len;
		argv = (char**)(ssp - argc);
		/* space for the argument-pointer */
		ssp -= argc;
		/* start for the arguments */
		str = (char*)ssp;
		for(i = 0; i < argc; i++) {
			/* start <len> bytes backwards */
			len = strlen(args) + 1;
			str -= len;
			/* store arg-pointer and copy arg */
			argv[i] = str;
			memcpy(str,args,len);
			/* to next */
			args += len;
		}
		/* ensure that we don't overwrites the characters */
		ssp = (uint64_t*)(((uintptr_t)str & ~(sizeof(uint64_t) - 1)) - sizeof(uint64_t));
	}
	*ssp = info->stackBegin;

	/* store argc and argv */
	rsp[1] = argc;
	rsp[2] = (uint64_t)argv;

	/* add TLS args and entrypoint; use prog-entry here because its always the entry of the
	 * program, not the dynamic-linker */
	uenv_addArgs(t,info,rsp,ssp,entryPoint,0,false);
	return true;
}

bool uenv_setupThread(const void *arg,uintptr_t tentryPoint) {
	uint64_t *rsp,*ssp;
	sThread *t = thread_getRunning();
	/* for idle, there is nothing to do */
	if(t->tid == IDLE_TID)
		return true;

	sStartupInfo sinfo;
	sinfo.progEntry = t->proc->entryPoint;
	sinfo.linkerEntry = 0;
	sinfo.stackBegin = 0;

	/*
	 * Initial software stack:
	 * +------------------+  <- top
	 * |     stack-end    |  used for UNSAVE
	 * +------------------+
	 *
	 * Registers:
	 * $1 = arg
	 * $4 = entryPoint (0 for initial thread, thread-entrypoint for others)
	 * $5 = TLSStart (0 if not present)
	 * $6 = TLSSize (0 if not present)
	 */

	/* the thread has to perform an UNSAVE at the beginning to establish the initial state.
	 * therefore, we have to prepare this again with the ELF-finisher. additionally, we have to
	 * take care that we use elf_finishFromMem() for boot-modules and elf_finishFromFile() other-
	 * wise. (e.g. fs depends on rtc -> rtc can't read it from file because fs is not ready) */
	if(t->proc->pid == DISK_PID || t->proc->pid == RTC_PID || t->proc->pid == FS_PID) {
		size_t i;
		const sBootInfo *info = boot_getInfo();
		for(i = 1; i < info->progCount; i++) {
			if(info->progs[i].id == t->proc->pid) {
				if(elf_finishFromMem((void*)info->progs[i].start,info->progs[i].size,&sinfo) < 0)
					return false;
				break;
			}
		}
	}
	else {
		/* TODO well, its not really nice that we have to read this stuff again for every started
		 * thread :/ */
		/* every process has a text-region from his binary */
		sVMRegion *textreg = vmm_getRegion(t->proc,RNO_TEXT);
		ssize_t res;
		sElfEHeader ehd;
		if(textreg->binFile < 0) {
			textreg->binFile = vfs_real_openInode(t->proc->pid,VFS_READ,textreg->reg->binary.ino,
					textreg->reg->binary.dev);
			if(textreg->binFile < 0) {
				log_printf("[LOADER] Unable to open path '%s': %s\n",t->proc->command,
						strerror(textreg->binFile));
				return false;
			}
		}

		/* seek to header */
		if(vfs_seek(t->proc->pid,textreg->binFile,0,SEEK_SET) < 0) {
			log_printf("[LOADER] Unable to seek to header of '%s'\n",t->proc->command);
			return false;
		}

		/* read the header */
		if((res = vfs_readFile(t->proc->pid,textreg->binFile,&ehd,sizeof(sElfEHeader))) !=
				sizeof(sElfEHeader)) {
			log_printf("[LOADER] Reading ELF-header of '%s' failed: %s\n",
					t->proc->command,strerror(res));
			return false;
		}

		res = elf_finishFromFile(textreg->binFile,&ehd,&sinfo);
		if(res < 0)
			return false;
	}

	/* get register-stack */
	vmm_getRegRange(t->proc,t->stackRegions[0],(uintptr_t*)&rsp,NULL);
	/* get software-stack */
	vmm_getRegRange(t->proc,t->stackRegions[1],NULL,(uintptr_t*)&ssp);
	/* extend the stack if necessary */
	if(thread_extendStack((uintptr_t)ssp - 8) < 0)
		return false;
	/* will handle copy-on-write */
	paging_isRangeUserWritable((uintptr_t)ssp - 8,8);

	/* store location to UNSAVE from and the thread-argument */
	*--ssp = sinfo.stackBegin;
	rsp[1] = (uint64_t)arg;

	/* add TLS args and entrypoint */
	uenv_addArgs(t,&sinfo,rsp,ssp,t->proc->entryPoint,tentryPoint,true);

	/* change rS and rO in the save-area, to set the correct values for the new thread */
	uint64_t *frame = t->kstackEnd;
	int rg = frame[-1] >> 56;
	size_t rsOff = -(13 + (255 - rg) + 2);
	uint64_t oldrS = frame[rsOff];
	frame[rsOff] = (uintptr_t)rsp + (oldrS & (PAGE_SIZE - 1));	/* rS */
	frame[rsOff - 1] = (uintptr_t)rsp;							/* rO */
	return true;
}

static void uenv_addArgs(sThread *t,const sStartupInfo *info,uint64_t *rsp,uint64_t *ssp,
		uintptr_t entry,uintptr_t tentry,bool thread) {
	/* put address and size of the tls-region on the stack */
	if(t->tlsRegion >= 0) {
		uintptr_t tlsStart,tlsEnd;
		vmm_getRegRange(t->proc,t->tlsRegion,&tlsStart,&tlsEnd);
		rsp[5] = tlsStart;
		rsp[6] = tlsEnd - tlsStart;
	}
	else {
		/* no tls */
		rsp[5] = 0;
		rsp[6] = 0;
	}
	rsp[4] = thread ? tentry : 0;

	/* setup sp and fp in kernel-stack; we pass the location to unsave from with it */
	uint64_t *frame = t->kstackEnd;
	frame[-(12 + 2 + 1)] = (uint64_t)ssp;
	frame[-(12 + 2 + 2)] = (uint64_t)ssp;
	/* set them in the stack to unsave from in user-space */
	int rg = *(uint64_t*)info->stackBegin >> 56;
	rsp[6 + (255 - rg)] = (uint64_t)ssp;
	rsp[6 + (255 - rg) + 1] = (uint64_t)ssp;

	/* setup start */
	cpu_setKSpecials(0,entry,(ulong)1 << 63,0,0);
}
