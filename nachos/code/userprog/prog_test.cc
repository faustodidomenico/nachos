#include "machine/console.hh"
#include "threads/synch.hh"
#include "threads/system.hh"
// #include "synch_console.hh"
#include <stdio.h>


/// Run a user program.
///
/// Open the executable, load it into memory, and jump to it.
void
StartProcess(const char *filename)
{
    ASSERT(filename != nullptr);

    OpenFile *executable = fileSystem->Open(filename);
    if (executable == nullptr) {
        printf("Unable to open file %s\n", filename);
        return;
    }

    AddressSpace *space = new AddressSpace(executable, currentThread->GetProcessId());
    currentThread->space = space;

    // delete executable;

    space->InitRegisters();  // Set the initial register values.
    space->RestoreState();   // Load page table register.

    machine->Run();  // Jump to the user progam.
    ASSERT(false);   // `machine->Run` never returns; the address space
                     // exits by doing the system call `Exit`.
}

/// Data structures needed for the console test.
///
/// Threads making I/O requests wait on a `Semaphore` to delay until the I/O
/// completes.

static Console   *console;
static Semaphore *readAvail;
static Semaphore *writeDone;
// static SynchConsole *synchConsole;

/// Console interrupt handlers.
///
/// Wake up the thread that requested the I/O.

static void
ReadAvail(void *arg)
{
    readAvail->V();
}

static void
WriteDone(void *arg)
{
    writeDone->V();
}

/// Test the console by echoing characters typed at the input onto the
/// output.
///
/// Stop when the user types a `q`.
void
ConsoleTest(const char *in, const char *out)
{
    console   = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);

    for (;;) {
        readAvail->P();        // Wait for character to arrive.
        char ch = console->GetChar();
        console->PutChar(ch);  // Echo it!
        writeDone->P();        // Wait for write to finish.
        if (ch == 'q')
            return;  // If `q`, then quit.
    }
}

void
SynchConsoleTest()
{
    synchConsole   = new SynchConsole();
    // readAvail = new Semaphore("read avail", 0);
    // writeDone = new Semaphore("write done", 0);

    for (;;) {
        char c = synchConsole->Read();
        synchConsole->Write(c);  // Echo it!
        if (c == 'q')
            return;  // If `q`, then quit.
    }
}