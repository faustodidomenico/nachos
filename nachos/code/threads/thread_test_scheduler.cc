#include "system.hh"
#include "synch.hh"

#include <stdio.h>

Lock *lck = new Lock("Lock priority test");

void SimpleThreadDelayed(void *args){
    printf("Running thread %s\n",currentThread->GetName());
    for(int i=0; i< 100000; i++);
    
}

void SimpleThreadPrint(void *args){
    printf("Running thread %s\n",currentThread->GetName());
}

void LowThreadLock(void *args){
    printf("Thread %s, trying to aquire %s lock. \n", currentThread->GetName(),lck->GetName());
    lck->Acquire();
    printf("Thread %s, acquired %s lock. \n", currentThread->GetName(),lck->GetName());
    currentThread->Yield();
    lck->Release();
    printf("Thread %s, released %s lock. \n", currentThread->GetName(),lck->GetName());
}

void HighThreadLock(void *args){
    printf("Thread %s, trying to aquire %s lock. \n", currentThread->GetName(),lck->GetName());
    lck->Acquire();
    printf("Thread %s, acquired %s lock. \n", currentThread->GetName(),lck->GetName());
    lck->Release();
    printf("Thread %s, released %s lock. \n", currentThread->GetName(),lck->GetName());
}

void SimpleThreadYield(void * args){
    for(int i=0; i<5; i++){
        printf("Thread %s con prioridad: %d en la iteracion %d\n",currentThread->GetName(),currentThread->GetPriority(),i);
        currentThread->Yield();
    }
}

void SimplePriorityTest(){
    DEBUG('t', "Entering simple priority test...\n");
    Thread *t1 = new Thread("1",false,0);
    Thread *t2 = new Thread("2",false,1);
    Thread *t3 = new Thread("3",false,3);
    Thread *t4 = new Thread("4",false,2);
    Thread *t5 = new Thread("5",false,3);

    t1->Fork(SimpleThreadDelayed,nullptr);
    t2->Fork(SimpleThreadPrint,nullptr);
    t3->Fork(SimpleThreadPrint,nullptr);
    t4->Fork(SimpleThreadPrint,nullptr);
    t5->Fork(SimpleThreadPrint,nullptr);
}

void PriorityInversionTest(){
    DEBUG('t', "Entering priority inversion test...\n");
    Thread *t1 = new Thread("T1",false,2);
    Thread *t2 = new Thread("T2",false,4);
    Thread *t3 = new Thread("T3",false,5);
    Thread *t4 = new Thread("T4",false,4);
    Thread *t5 = new Thread("T5",true,8);

    t1->Fork(LowThreadLock,nullptr);
    currentThread->Yield();
    t2->Fork(SimpleThreadYield,nullptr);
    t3->Fork(SimpleThreadYield,nullptr);
    t4->Fork(SimpleThreadYield,nullptr);
    t5->Fork(HighThreadLock,nullptr);
    t5->Join();
}

void ThreadTestScheduler(){

    // SimplePriorityTest();
    PriorityInversionTest();
}

/*  
SimpleThreadLock()

SimpleThreadYield
    2 -> Toma el bloqueo. Y no termina...
    4,5 -> Dos procesos normales.
    7 -> Quiere tomar el bloqueo que tomo 2.

    2, 4, 5, 7

    Promote(int newPriority);
*/ 