/// Simple test case for the threads assignment.
///
/// Create several threads, and have them context switch back and forth
/// between themselves by calling `Thread::Yield`, to illustrate the inner
/// workings of the thread system.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "system.hh"
#include "synch.hh"
#include "thread_test_channel.cc"
#include "thread_test_join.cc"
#include "thread_test_scheduler.cc"

#include <stdio.h>
#include <string.h>

#ifdef SEMAPHORE_TEST
Semaphore *sem = new Semaphore("Semaphore test.", 3);
#endif

#ifdef LOCK_TEST
Lock *lck = new Lock("Lock test.");
#endif

#ifdef CONDITION_TEST
Lock *cndLock = new Lock("Condition lock.");
Condition *cnd = new Condition("Condition 1", cndLock);
#endif

/// Loop 10 times, yielding the CPU to another ready thread each iteration.
///
/// * `name` points to a string with a thread name, just for debugging
///   purposes.
void
SimpleThread(void *name_)
{
    // Reinterpret arg `name` as a string.
    char *name = (char *) name_;

    // If the lines dealing with interrupts are commented, the code will
    // behave incorrectly, because printf execution may cause race
    // conditions.
    #ifdef SEMAPHORE_TEST
    DEBUG('s', "The thread (%s) made a P() call.\n", name);
    sem->P();
    #endif

    #ifdef LOCK_TEST
    DEBUG('l', "The thread (%s) made an Acquire call.\n", name);
    lck->Acquire();
    #endif

    #ifdef CONDITION_TEST
    // cnd->Signal();
    cndLock->Acquire();
    DEBUG('l', "The thread (%s) waits in condition %s.\n", name, cnd->GetName());
    
    #endif

    for (unsigned num = 0; num < 10; num++) {

        printf("*** Thread `%s` is running: iteration %u\n", name, num);
        currentThread->Yield();

        #ifdef CONDITION_TEST
        if(num == 3) cnd->Broadcast();
        if(num == 7) cnd->Wait();
        #endif
    }

    printf("!!! Thread `%s` has finished\n", name);

    #ifdef SEMAPHORE_TEST
    DEBUG('s', "The thread (%s) made a V() call.\n", name);
    sem->V();
    #endif

    #ifdef LOCK_TEST
    DEBUG('l', "The thread (%s) made a Release call.\n", name);
    lck->Release();
    #endif

    #ifdef CONDITION_TEST
    // DEBUG('l', "The thread (%s) made a Signal in condition %s.\n", name, cnd->GetName());
    cndLock->Release();
    #endif
}

/// Set up a ping-pong between several threads.
///
/// Do it by launching ten threads which call `SimpleThread`, and finally
/// calling `SimpleThread` ourselves.
void
ThreadTest()
{
    DEBUG('t', "Entering thread test\n");
    // for(int i=2;i <= 5; i++){
    // char *name = new char [64];
    // sprintf(name, "%d", i);
    // Thread *newThread = new Thread(name);
    // newThread->Fork(SimpleThread, (void *) name);
    // }
    // SimpleThread((void *) "1");

    #ifdef SEMAPHORE_TEST
    // sem->~Semaphore();
    #endif

    #ifdef LOCK_TEST
    // delete lck;
    #endif

    #ifdef CHANNEL_TEST
        ThreadTestChannel();
    #endif

    #ifdef JOIN_TEST
        // ThreadTestJoin();
    #endif
    ThreadTestScheduler();
}
