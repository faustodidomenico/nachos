#include "system.hh"
#include "synch.hh"

#include <stdio.h>


Channel *chn = new Channel("Test channel",100);

void
SendTestMessage(void *arg){
    chn->Send(15);
    // chn->Send(16);
    // chn->Send(17);
    printf("Thread %s sended message in channel %s : %d\n",currentThread->GetName(), chn->GetName(), 15);
}

void
ReceiveTestMessage(void *arg){
    int *response = new int[100];
    
    for(int i=0; i<4; i++){
        chn->Receive(response);
        printf("Thread %s received message in channel %s : %d\n",currentThread->GetName(), chn->GetName(), *response);
    }

}

void
ThreadTestChannel(){
    
    DEBUG('t', "Entering channel thread test\n");
    
    
    Thread *sender1 = new Thread("Sender1",false);
    Thread *sender2 = new Thread("Sender2",false);
    Thread *sender3 = new Thread("Sender3",false);
    Thread *receiver1 = new Thread("Receiver1",false);
    Thread *receiver2 = new Thread("Receiver2",false);
    sender1->Fork(SendTestMessage, nullptr);
    sender2->Fork(SendTestMessage, nullptr);
    sender3->Fork(SendTestMessage, nullptr);
    receiver1->Fork(ReceiveTestMessage, nullptr);
    receiver2->Fork(ReceiveTestMessage, nullptr);
}