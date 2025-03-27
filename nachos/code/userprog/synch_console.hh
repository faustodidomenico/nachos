// #ifdef USER_PROGRAM

#include "machine/console.hh"
#include "threads/synch.hh"

class SynchConsole{
public:
    // 
    SynchConsole();
    ~SynchConsole();
        
    // 
    void ReadAvailSynch();
    void WriteDoneSynch();
        
    // 
    char Read();
    void Write(char data);
    
private:
    // Raw console device.
    Console *console;
    // To sincronize requesting thread
    // with the interrupt handler.
    Semaphore *readAvail;
    
    Semaphore *writeDone;

    Lock *writerLock;
    Lock *readerLock;
};
