// /// Copyright (c) 2019-2020 Docentes de la Universidad Nacional de Rosario.
// /// All rights reserved.  See `copyright.h` for copyright notice and
// /// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "lib/utility.hh"
#include "threads/system.hh"

#define MAX_PAGE_FAULTS 5

bool SafeReadMem(unsigned userAddress, unsigned size, int* buffer){

    if(machine->ReadMem(userAddress, size, buffer, true)) return true;

    for(unsigned i = 1; i < MAX_PAGE_FAULTS; i++)
        if(machine->ReadMem(userAddress, size, buffer,false))
            return true;
    return false;
}

bool SafeWriteMem(unsigned userAddress, unsigned size, int value){

    if(machine->WriteMem(userAddress, size, value)) return true;

    for(unsigned i = 1; i < MAX_PAGE_FAULTS; i++)
        if(machine->WriteMem(userAddress, size, value))
            return true;
    return false;
}


void ReadBufferFromUser(int userAddress, char *outBuffer,
                        unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outBuffer != nullptr);
    ASSERT(byteCount != 0);
    
    for(unsigned count = 0; count < byteCount; count++){
        ASSERT(SafeReadMem(userAddress++, 1, (int*) outBuffer++));
    }
}


bool ReadStringFromUser(int userAddress, char *outString,
                        unsigned maxByteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outString != nullptr);
    ASSERT(maxByteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        count++;
        ASSERT(SafeReadMem(userAddress++, 1, &temp));
        *outString = (unsigned char) temp;
    } while (*outString++ != '\0' && count < maxByteCount);

    return *(outString - 1) == '\0';
}

void WriteBufferToUser(const char *buffer, int userAddress,
                       unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(buffer != nullptr);
    ASSERT(byteCount != 0);

    for(unsigned count = 0; count < byteCount; count++, buffer++ ){
        ASSERT(SafeWriteMem(userAddress++, 1, *buffer));
    }
}

void WriteStringToUser(const char *string, int userAddress)
{
    ASSERT(userAddress != 0);
    ASSERT(string != nullptr);
    
     while (*string != '\0'){
        ASSERT(SafeWriteMem(userAddress++, 1, *string++));
    }
}