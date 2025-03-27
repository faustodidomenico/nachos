/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "args.hh"
#include "lib/utility.hh"
#include <stdio.h>

static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

void
StartProcess(void *args){
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();

    if(args){
        char **argsv = (char**) args;
        
        DEBUG('e',"Arguments: \n");
        for(int i=0; argsv[i] != nullptr; i++)
            DEBUG('e',"ARG %d : %s\n", i, argsv[i]);

        int argc = WriteArgs(argsv);
        int ARGSOFFSET = 16;
        int argsAddr = machine->ReadRegister(STACK_REG);
        machine->WriteRegister(STACK_REG, argsAddr - ARGSOFFSET);
        
        machine->WriteRegister(4,argc);
        machine->WriteRegister(5,argsAddr);
    }
    machine->Run();
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)

/// This implementeation must be BULLET PROOF. It can't break NACHOS.
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {
        case SC_HALT:
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;

        // int Create(const char *name);
        case SC_CREATE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0)
                DEBUG('e', "Error: address to filename string is null.\n");

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename))
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);

            DEBUG('e', "`Create` requested for file `%s`.\n", filename);

            bool succes = fileSystem->Create(filename,0);

            if(succes)
                DEBUG('e', "File `%s` created succesfully.\n", filename);
            else
                DEBUG('e', "File `%s` is already in this directory .\n", filename);    

            break;
        }


        // int Close(OpenFileId id);
        case SC_CLOSE: {
            int fid = machine->ReadRegister(4);
            DEBUG('e', "`Close` requested for id %u.\n", fid);
            
            machine->WriteRegister(2,-1);
            if(fid < 2){
                DEBUG('e', "ERROR: Invalid file ID.\n");
                break;
            }
            else if (!currentThread->HasOpenFile(fid)){
                DEBUG('e', "ERROR: The file %d is not opened.\n", fid);
                break;
            } else{
                OpenFile *file = currentThread->GetFile(fid);
                delete file;
                currentThread->RemoveFile(fid);
                machine->WriteRegister(2, 1);
                break;
            }
        }

        case SC_READ: {
            int usrAddr    = machine->ReadRegister(4);
            int size       = machine->ReadRegister(5);
            OpenFileId fid = machine->ReadRegister(6);
            int pos        = machine->ReadRegister(7);
            int count      = 0;
            
            if(size <= 0){
                DEBUG('e', "Error: Size must be greater than 0.\n", size);
            }
            else if(fid < 0){
                DEBUG('e',"Error: File ID must be greater than 0 and distinct of 1.\n");
            } 
            
            else {    
                char buffer[size+1];
                DEBUG('e', "`Read` requested for file id %u.\n", fid);
                if(fid == CONSOLE_INPUT){
                    for(; count < size; count++) buffer[count] = synchConsole->Read();
                    WriteBufferToUser(buffer, usrAddr, size);
                }
                else if(fid == CONSOLE_OUTPUT){
                    DEBUG('e',"Reading from standard output not supported.\n");    
                } else {
                    OpenFile *file = currentThread->GetFile(fid);
                    if(file == nullptr){
                        DEBUG('e', "Error: file with %d was not opened correctly.\n", fid);
                    } else {
                        if(pos != -1){
                            count = file->ReadAt(buffer, size, pos);

                        } else{
                            count = file->Read(buffer, size);
                        }
                        if(count < size)
                            DEBUG('e', "Error: Expecting %d bytes from file %d and read %d bytes\n",size,fid,count);
                        else{
                            WriteBufferToUser(buffer, usrAddr, size);
                        }
                    }
                }                                 
            }
            machine->WriteRegister(2, count);
            break;
        } 
        
        /// Write `size` bytes from `buffer` to the open file.
        // int Write(const char *buffer, int size, OpenFileId id);
        case SC_WRITE: {
            int usrAddr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            OpenFileId fid = machine->ReadRegister(6);
            int count = 0;

            if(size <= 0){
                DEBUG('e', "Error: Size must be greater than 0.\n", size);
            }
            else if(fid < CONSOLE_OUTPUT){
                DEBUG('e',"Error: File ID must be greater than 0. (fid: %d)\n", fid);
            }             
            else{
            DEBUG('e',"Write requested, file id: %d\n",fid);

            char buffer[size];
            ReadBufferFromUser(usrAddr, buffer, size);

            if(fid == CONSOLE_OUTPUT){
                for (; count < size; count++){
                    synchConsole->Write(buffer[count]);
                }                
            } else{
                count = currentThread->GetFile(fid)->Write(buffer,size);
            }
            }  
        machine->WriteRegister(2,count);
        break;
        }

        case SC_OPEN: {
            int filenameAddr = machine->ReadRegister(4);
            char filename[FILE_NAME_MAX_LEN + 1];
            
            OpenFileId fid = -1;

            if (filenameAddr == 0)
                DEBUG('e', "Error: address to filename string is null.\n");
            else if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)){
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                        FILE_NAME_MAX_LEN);
            } else{
                OpenFile *file = fileSystem->Open(filename);
                if(file == nullptr){
                    DEBUG('e',"Error: file with name %s doesn't exists.\n",filename);
                }else {
                    fid = currentThread->AddFile(file);
                    
                    if(fid < 0) DEBUG('e', "Error: Thread file descriptor table is full. \n");

                    else DEBUG('e',"Opened file id: %d\n",fid);
                }                
            }
            machine->WriteRegister(2,fid);
            break;
        }
        
        /// This user program is done (`status = 0` means exited normally).
        case SC_EXIT:{
            int status = machine->ReadRegister(4);
            DEBUG('e', "Exited thread with status %d\n", status);            
                #ifdef VMEM
                char *buffer = new char [20];
                sprintf(buffer,"SWAP.%d",currentThread->GetProcessId());
                fileSystem->Remove(buffer);
                delete currentThread->space->swapFile; 
                delete [] buffer;
            #endif
            currentThread->Finish();
            break;
        }

        /// Only return once the the user program `id` has finished.
        /// Return the exit status.
        case SC_JOIN: {
            SpaceId spaceId = machine->ReadRegister(4);

            machine->WriteRegister(2,1);
            if(spaceId < 0 || !processTable->HasKey(spaceId)){
                DEBUG('t', "Error: invalid proccess id.\n");
            }
            else {
                Thread *thread = processTable->Get(spaceId);
                if(!thread->IsJoinable()){
                    DEBUG('t', "Thread %s is not joinable.\n",thread->GetName());
                } 
                else {
                    thread->Join();
                    // Child process finished correctly.
                    machine->WriteRegister(2,0);
                }
            }
            break;
        }

        /// Run the executable, stored in the Nachos file `name`, and return the
        /// address space identifier.
        // En args[0] viene el nombre del archivo.
        //SpaceId Exec(char* name, char **args, int isJoinable);// HALT: Llama al metodo HALT del sistema de interrupciones de NACHOS.

        case SC_EXEC: {
            int filenameAddr = machine->ReadRegister(4);
            int argsvAddr = machine->ReadRegister(5);
            int isJoinable = machine->ReadRegister(6);
            char filename[FILE_NAME_MAX_LEN+1];
            char **argsv = nullptr;

            machine->WriteRegister(2,-1);

            if(argsvAddr != 0){
                argsv = SaveArgs(argsvAddr);
            }

            if(!filenameAddr){
                DEBUG('e', "Error: filename address can't be null.\n");
            }
            else if (!ReadStringFromUser(filenameAddr, filename, sizeof filename))
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);

            else {
                DEBUG('e',"`exec` requested for filename: %s.\n",filename);
                OpenFile *executable = fileSystem->Open(filename);
                if(executable == nullptr){
                    DEBUG('e',"Error: requested file with filename: %s not found.\n",filename);
                }
                else {
                    unsigned dirSector = 1;
                    #ifdef DIRECTORY
                        dirSector = currentThread->GetDirSector();
                    #endif
                    
                    Thread *t = new Thread(filename,(bool) isJoinable,currentThread->GetPriority(),dirSector);
                
                    SpaceId pid = t->GetProcessId();
                    AddressSpace *addrSpace = new AddressSpace(executable,pid);
                    if(addrSpace == nullptr){
                        DEBUG('e',"Error: not enough memory for the new thread.\n");
                    }
                    else{
                        t->space = addrSpace;
                        DEBUG('e',"New thread created to run the executable %s with pid: %d and space address: %p\n", filename, t->GetProcessId(),addrSpace);
                        machine->WriteRegister(2,pid);
                        t->Fork(StartProcess, argsv);
                    }
                }
            }   
            break;
        }

        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);

    }

    IncrementPC();
}

#ifdef USE_TLB
void
DebugTLBEntryInfo(TranslationEntry entry){
    DEBUG('k', "VP: %d - PP: %d - VALID: %d\n",entry.virtualPage, entry.physicalPage, entry.valid);
}

static void
PageFaultHandler(ExceptionType _et){
     unsigned vaddr = machine->ReadRegister(BAD_VADDR_REG);
     unsigned vpn = vaddr / PAGE_SIZE;
     unsigned i = machine->GetMMU()->GetNextIdx();

    TranslationEntry *tlb = machine->GetMMU()->tlb;
    TranslationEntry *pageTable = currentThread->space->GetPageTable();

    if(!pageTable[vpn].valid){
        DEBUG('k',"Requested page is invalid. Loading page...\n");
        tlb[i] = currentThread->space->LoadPage(vpn);
    } else{
        DEBUG('k',"Requested page is valid. Copying page to user's page table...\n");
	    currentThread->space->SaveState();
        tlb[i] = pageTable[vpn];
    }
}

static void
ReadOnlyHandler(ExceptionType _et){
    currentThread->Finish();
}

#endif

/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    #ifdef USE_TLB
    machine->SetHandler(READ_ONLY_EXCEPTION,     &ReadOnlyHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &PageFaultHandler);
    #else
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &DefaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &DefaultHandler);
    #endif
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
    
    // En los registros vendria el numero de senal y el pid al que se le envia.
    // machine->SetHandler(SIGNAL_EXCEPTION, &SignalHandler);
}