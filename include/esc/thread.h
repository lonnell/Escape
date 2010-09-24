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

#ifndef THREAD_H_
#define THREAD_H_

#include <esc/common.h>

/* the thread-entry-point-function */
typedef int (*fThreadEntry)(void *arg);

/* the events we can wait for */
#define EV_NOEVENT				0			/* just wakeup on signals */
#define EV_CLIENT				(1 << 0)	/* if there's a client to be served */
#define EV_RECEIVED_MSG			(1 << 1)	/* if a driver has a msg for us */
#define EV_DATA_READABLE		(1 << 3)	/* if we can read from a driver (data available) */
#define EV_USER1				(1 << 14)	/* an event we can send */
#define EV_USER2				(1 << 15)	/* an event we can send */

#define LOCK_EXCLUSIVE			1
#define LOCK_KEEP				2

typedef u32 tULock;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @return the id of the current thread
 */
tTid gettid(void);

/**
 * @return the number of threads in the current process
 */
u32 getThreadCount(void);

/**
 * Starts a new thread
 *
 * @param entryPoint the entry-point of the thread
 * @param arg the argument to pass (just the pointer)
 * @return the new tid, < 0 if failed
 */
s32 startThread(fThreadEntry entryPoint,void *arg) A_CHECKRET;

/**
 * The syscall exit
 *
 * @param errorCode the error-code for the parent
 */
void _exit(s32 exitCode) A_NORETURN;

/**
 * @return the cpu-cycles of the current thread
 */
u64 getCycles(void);

/**
 * Releases the CPU (reschedule)
 */
void yield(void);

/**
 * Puts the current thread to sleep for <msecs> milliseconds. If interrupted, ERR_INTERRUPTED
 * is returned.
 *
 * @param msecs the number of milliseconds to wait
 * @return 0 on success
 */
s32 sleep(u32 msecs);

/**
 * Puts the current thread to sleep until one of the given events occurrs. Note that you will
 * always be waked up for signals!
 *
 * @param events the events on which you want to wake up
 * @return 0 on success and a negative error-code if failed
 */
s32 wait(u32 events);

/**
 * Notifies the given thread about the given events. If it was waiting for them, it will be
 * waked up.
 *
 * @param tid the thread-id to notify
 * @param events the events on which you want to wake up
 * @return 0 on success and a negative error-code if failed
 */
s32 notify(tTid tid,u32 events);

/**
 * Joins a thread, i.e. it waits until a thread with given tid has died (from the own process)
 *
 * @param tid the thread-id (0 = wait until all other threads died)
 * @return 0 on success
 */
s32 join(tTid tid);

/**
 * Suspends a thread from the own process. That means it is blocked until resume() is called.
 *
 * @param tid the thread-id
 * @return 0 on success
 */
s32 suspend(tTid tid);

/**
 * Resumes a thread from the own process that has been suspended previously
 *
 * @param tid the thread-id
 * @return 0 on success
 */
s32 resume(tTid tid);

/**
 * Sets the thread-value with given key to given value (for the current thread)
 *
 * @param key the key
 * @param val the value
 * @return true if successfull
 */
bool setThreadVal(u32 key,void *val);

/**
 * Retrieves the value of the thread-value with given key (for the current thread)
 *
 * @param key the key
 * @return the value or NULL if not found
 */
void *getThreadVal(u32 key);

/**
 * Aquires a process-local lock with ident. You can specify with the flags whether it should
 * be an exclusive lock and whether it should be free'd if it is no longer needed (no waits atm)
 *
 * @param ident to identify the lock
 * @param flags flags (LOCK_*)
 * @return 0 on success
 */
s32 lock(u32 ident,u16 flags);

/**
 * Aquires a process-local lock in user-space. If the given lock is in use, the process waits
 * (actively, i.e. spinlock) until the lock is unused.
 *
 * @param lock the lock
 */
void locku(tULock *lock);

/**
 * Aquires a global lock with given ident. You can specify with the flags whether it should
 * be an exclusive lock and whether it should be free'd if it is no longer needed (no waits atm)
 *
 * @param ident to identify the lock
 * @return 0 on success
 */
s32 lockg(u32 ident,u16 flags);

/**
 * First it releases the specified process-local lock. After that it blocks the thread until
 * one of the given events occurrs. This gives user-threads the chance to ensure that a
 * notify() arrives AFTER a thread has called wait. So they can do:
 * thread1:
 *  lock(...);
 *  ...
 *  waitUnlock(...);
 *
 * thread2:
 *  lock(...);
 *  notify(thread1,...);
 *  unlock(...);
 *
 *  @param events the events to wait for
 *  @param ident the ident to unlock
 *  @return 0 on success
 */
s32 waitUnlock(u32 events,u32 ident);

/**
 * First it releases the specified global lock. After that it blocks the thread until one of the
 * given events occurrs. This gives user-threads the chance to ensure that a notify() arrives
 * AFTER a thread has called wait. So they can do:
 * thread1:
 *  lock(...);
 *  ...
 *  waitUnlock(...);
 *
 * thread2:
 *  lock(...);
 *  notify(thread1,...);
 *  unlock(...);
 *
 *  @param events the events to wait for
 *  @param ident the ident to unlock
 *  @return 0 on success
 */
s32 waitUnlockg(u32 events,u32 ident);

/**
 * Releases the process-local lock with given ident
 *
 * @param ident to identify the lock
 * @return 0 on success
 */
s32 unlock(u32 ident);

/**
 * Releases the process-local lock that is locked in user-space
 *
 * @param lock the lock
 */
void unlocku(tULock *lock);

/**
 * Releases the global lock with given ident
 *
 * @param ident to identify the lock
 * @return 0 on success
 */
s32 unlockg(u32 ident);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_H_ */
