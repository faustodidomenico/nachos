#include "system.hh"
#include "synch.hh"

#include <stdio.h>

void ThreadHijo(void *arg){
    printf("I'm the joined thread (I'm %s)\n", currentThread->GetName());
    for(int i=0;i<10000;i++);
    printf("Finishing joined thread (I'm %s)\n", currentThread->GetName());
}

void ThreadTestJoin(){
    Thread *t = new Thread("Joinable Thread", true);
    t->Fork(ThreadHijo,nullptr);
    printf("Joining thread (I'm %s)\n", currentThread->GetName());
    t->Join();
    printf("Joined thread (I'm %s)\n", currentThread->GetName());
}