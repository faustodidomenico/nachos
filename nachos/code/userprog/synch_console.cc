// #ifdef USER_PROGRAM

#include "synch_console.hh"

static void
ReadAvailS(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *c = (SynchConsole *) arg;
    c->ReadAvailSynch();
}


static void
WriteDoneS(void *arg)
{
    
    ASSERT(arg != nullptr);
    SynchConsole *c = (SynchConsole *) arg;
    c->WriteDoneSynch();
    
}


SynchConsole::SynchConsole(){
    console = new Console(nullptr,nullptr,ReadAvailS,WriteDoneS,this);
    readAvail = new Semaphore("ReadAvail Semaphore",0);
    writeDone = new Semaphore("WriteDone Semaphore",0);
    readerLock = new Lock("Reader lock");
    writerLock = new Lock("Writer lock");
}

SynchConsole::~SynchConsole(){
    delete console;
    delete readAvail;
    delete writeDone;
    delete writerLock;
    delete readerLock;
}

void
SynchConsole::Write(char c){
    writerLock->Acquire();
        console->PutChar(c);
        writeDone->P();
    writerLock->Release();
}

char
SynchConsole::Read(){
    char c;

    readerLock->Acquire();
        readAvail->P();
        c = console->GetChar();
    readerLock->Release();

    return c;
}

/// Console interrupt handler.  Wake up any thread waiting for the console
/// read request to finish.
void
SynchConsole::ReadAvailSynch(){
    readAvail->V();

}

/// Console interrupt handler.  Wake up any thread waiting for the console
/// write request to finish.
void
SynchConsole::WriteDoneSynch(){
    writeDone->V();
}

// #endif