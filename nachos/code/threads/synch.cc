/// Routines for synchronizing threads.
///
/// Three kinds of synchronization routines are defined here: semaphores,
/// locks and condition variables (the implementation of the last two are
/// left to the reader).
///
/// Any implementation of a synchronization routine needs some primitive
/// atomic operation.  We assume Nachos is running on a uniprocessor, and
/// thus atomicity can be provided by turning off interrupts.  While
/// interrupts are disabled, no context switch can occur, and thus the
/// current thread is guaranteed to hold the CPU throughout, until interrupts
/// are reenabled.
///
/// Because some of these routines might be called with interrupts already
/// disabled (`Semaphore::V` for one), instead of turning on interrupts at
/// the end of the atomic operation, we always simply re-set the interrupt
/// state back to its original value (whether that be disabled or enabled).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "synch.hh"
#include "system.hh"

/// Initialize a semaphore, so that it can be used for synchronization.
///
/// * `debugName` is an arbitrary name, useful for debugging.
/// * `initialValue` is the initial value of the semaphore.
Semaphore::Semaphore(const char *debugName, int initialValue)
{
    name  = debugName;
    value = initialValue;
    queue = new List<Thread *>;
}

/// De-allocate semaphore, when no longer needed.
///
/// Assume no one is still waiting on the semaphore!
Semaphore::~Semaphore()
{
    delete queue;
}

const char *
Semaphore::GetName() const
{
    return name;
}

/// Wait until semaphore `value > 0`, then decrement.
///
/// Checking the value and decrementing must be done atomically, so we need
/// to disable interrupts before checking the value.
///
/// Note that `Thread::Sleep` assumes that interrupts are disabled when it is
/// called.
void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(INT_OFF);
      // Disable interrupts.

    while (value == 0) {  // Semaphore not available.
        queue->Append(currentThread);  // So go to sleep.
        currentThread->Sleep();
    }
    value--;  // Semaphore available, consume its value.

    interrupt->SetLevel(oldLevel);  // Re-enable interrupts.
}

/// Increment semaphore value, waking up a waiter if necessary.
///
/// As with `P`, this operation must be atomic, so we need to disable
/// interrupts.  `Scheduler::ReadyToRun` assumes that threads are disabled
/// when it is called.
void
Semaphore::V()
{
    IntStatus oldLevel = interrupt->SetLevel(INT_OFF);

    Thread *thread = queue->Pop();
    if (thread != nullptr)
        // Make thread ready, consuming the `V` immediately.
        scheduler->ReadyToRun(thread);
    value++;

    interrupt->SetLevel(oldLevel);
}

/// Dummy functions -- so we can compile our later assignments.
///
/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Lock::Lock(const char *debugName)
{
    //  Set lock debugName
    name = debugName;

    // Initialize Semaphore
    sem = new Semaphore("Lock semaphore", 1);
    
    // There's no thread holding the lock.
    thread = NULL;
        
}

Lock::~Lock()
{
    // Should we free the semaphore's memory?
}

const char *
Lock::GetName() const
{
    return name;
}


// 
void
Lock::Acquire()
{
    DEBUG('l',"Trying to acquire lock by thread (%s)\n",currentThread->GetName());
    ASSERT(!IsHeldByCurrentThread());

    #ifdef PRIORITY_INVERSION
    
    // If there's a thread holding the lock and the thread that's triying to acquire
    // the lock haves a higher priority, the lock's thread get promoted to max priority.

    if(thread != nullptr && thread->GetPriority() < currentThread->GetPriority()){

        // Save the actual thread's priority in order to restore it after realising lock.
        threadPriority = thread->GetPriority();

        // Max priority of ReadyList.
        int maxPriority = scheduler->GetMaxPriority();
        
        // For the case that current thread have higher priority.
        int p = maxPriority > currentThread->GetPriority() ? maxPriority : currentThread->GetPriority();

        // Change priorities.
        scheduler->ChangePriority(p+1, thread);
    }

    #endif

    sem->P();
    threadPriority = currentThread->GetPriority();
    thread = currentThread;
}

void
Lock::Release()
{
    DEBUG('l',"Releasing lock by thread (%s)\n",currentThread->GetName());
    ASSERT(IsHeldByCurrentThread());

    #ifdef PRIORITY_INVERSION
    
    // If it's priority had been changed, restore it.
    if(thread->GetPriority() != threadPriority){
        int temp = currentThread->GetPriority();
        scheduler->ChangePriority(threadPriority, currentThread);
        threadPriority = temp;
    }
    #endif
    
    thread = nullptr;
    sem->V();


}

bool
Lock::IsHeldByCurrentThread() const
{  
    return thread == currentThread;
}

Condition::Condition(const char *debugName, Lock *conditionLock)
{
    // Set debug name.
    name = debugName;

    // Bound lock
    lck = conditionLock;

    // Initialize queue list.
    queue = new List<Semaphore *>;
}

Condition::~Condition()
{
    delete queue;
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
    ASSERT(lck->IsHeldByCurrentThread());
    // Use the semaphore to wait for the signal.
    Semaphore *condSem = new Semaphore("Condition Semaphore",0);

    // Add the semaphore to the list of the 
    queue->Append(condSem);
    
    // Release the lock.
    lck->Release();

    // Wait for signal.
    condSem->P();

    // Try to acquire the lock.
    lck->Acquire();
}

void
Condition::Signal()
{   
    if(!queue->IsEmpty()){        
        Semaphore *sem = queue->Pop();
        sem->V();
    }
}

void
Condition::Broadcast()
{
    if(!queue->IsEmpty())
        for(Semaphore* awaken = queue->Pop(); !queue->IsEmpty();awaken = queue->Pop(), awaken->V());
}

Channel::Channel(const char *debugName, int buffSize){
    name = debugName;

    buffer = new int[buffSize];
    
    semSender = new Semaphore("Sender semaphore",0);
    semReceiver = new Semaphore("Receiver semaphore",0);
    semBusy = new Semaphore("Busy semaphore",1);
}

Channel::~Channel(){
    delete buffer;
    delete semSender;
    delete semReceiver;
    delete semBusy;
}

const char *
Channel::GetName() const
{
    return name;
}

void
Channel::Send(int message){

    DEBUG('c', "Thread: %s is waiting for other threads to Send.\n", currentThread->GetName());
    semBusy->P(); 

    DEBUG('c', "Thread: %s writes the message.\n", currentThread->GetName());
    *buffer = message;

    semReceiver->V();

    DEBUG('c', "Thread: %s waits someone to receive the message.\n", currentThread->GetName());
    semSender->P();
}


void
Channel::Receive(int *message){
    
    semSender->V();
    semReceiver->P();
    *message = *buffer;
    semBusy->V();

}

