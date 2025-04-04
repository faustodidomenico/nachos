/// Simple test routines for the file system.
///
/// We implement:
///
/// Copy
///     Copy a file from UNIX to Nachos.
/// Print
///     Cat the contents of a Nachos file.
/// Perftest
///     A stress test for the Nachos file system read and write a really
///     really large file in tiny chunks (will not work on baseline system!)
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "lib/utility.hh"
#include "machine/disk.hh"
#include "machine/statistics.hh"
#include "threads/thread.hh"
#include "threads/system.hh"
//#include "path.hh"

#include <stdio.h>
#include <string.h>

class Path;
static const unsigned TRANSFER_SIZE = 10;  // Make it small, just to be
                                           // difficult.

/// Copy the contents of the UNIX file `from` to the Nachos file `to`.
void
Copy(const char *from, const char *to)
{
    ASSERT(from != nullptr);
    ASSERT(to != nullptr);

    // Open UNIX file.
    FILE *fp = fopen(from, "r");
    if (fp == nullptr) {
        printf("Copy: could not open input file %s\n", from);
        return;
    }

    // Figure out length of UNIX file.
    fseek(fp, 0, 2);
    int fileLength = ftell(fp);
    fseek(fp, 0, 0);

    DEBUG('f', "Copying file %s, size %u, to file %s\n",
          from, fileLength, to);

    // Create a Nachos file of the same length.
    if (!fileSystem->Create(to, fileLength)) {  // Create Nachos file.
        printf("Copy: could not create output file %s\n", to);
        fclose(fp);
        return;
    }

    OpenFile *openFile = fileSystem->Open(to);
    ASSERT(openFile != nullptr);

    // Copy the data in `TRANSFER_SIZE` chunks.
    char *buffer = new char [TRANSFER_SIZE];
    int amountRead;
    while ((amountRead = fread(buffer, sizeof(char),
                               TRANSFER_SIZE, fp)) > 0)
        openFile->Write(buffer, amountRead);
    delete [] buffer;

    // Close the UNIX and the Nachos files.
    delete openFile;
    fclose(fp);
}

/// Print the contents of the Nachos file `name`.
void
Print(const char *name)
{
    ASSERT(name != nullptr);

    OpenFile *openFile = fileSystem->Open(name);
    if (openFile == nullptr) {
        fprintf(stderr, "Print: unable to open file %s\n", name);
        return;
    }

    char *buffer = new char [TRANSFER_SIZE];
    int amountRead;
    while ((amountRead = openFile->Read(buffer, TRANSFER_SIZE)) > 0)
        for (unsigned i = 0; i < (unsigned) amountRead; i++)
            printf("%c", buffer[i]);

    delete [] buffer;
    delete openFile;  // close the Nachos file
}


/// Performance test
///
/// Stress the Nachos file system by creating a large file, writing it out a
/// bit at a time, reading it back a bit at a time, and then deleting the
/// file.
///
/// Implemented as three separate routines:
/// * `FileWrite` -- write the file.
/// * `FileRead` -- read the file.
/// * `PerformanceTest` -- overall control, and print out performance #'s.

static const char FILE_NAME[] = "TestFile";
static const char CONTENTS[] = "1234567890";
static const unsigned CONTENT_SIZE = sizeof CONTENTS - 1;
static const unsigned FILE_SIZE = CONTENT_SIZE * 5000;

static void
FileWrite()
{
    printf("Sequential write of %u byte file, in %u byte chunks\n",
           FILE_SIZE, CONTENT_SIZE);

    if (!fileSystem->Create(FILE_NAME, 0)) {
        fprintf(stderr, "Perf test: cannot create %s\n", FILE_NAME);
        return;
    }

    OpenFile *openFile = fileSystem->Open(FILE_NAME);
    if (openFile == nullptr) {
        fprintf(stderr, "Perf test: unable to open %s\n", FILE_NAME);
        return;
    }

    for (unsigned i = 0; i < FILE_SIZE; i += CONTENT_SIZE) {
        int numBytes = openFile->Write(CONTENTS, CONTENT_SIZE);
        if (numBytes < 10) {
            fprintf(stderr, "Perf test: unable to write %s\n", FILE_NAME);
            break;
        }
    }

    delete openFile;
}

#ifdef SYNCH_FSTEST
static const char* A = "1";

static OpenFile * openFile;
#endif

static void
FileRead()
{
    printf("Sequential read of %u byte file, in %u byte chunks\n",
           FILE_SIZE, CONTENT_SIZE);

    OpenFile *openFile = fileSystem->Open(FILE_NAME);
    if (openFile == nullptr) {
        fprintf(stderr, "Perf test: unable to open file %s\n", FILE_NAME);
        return;
    }

    char *buffer = new char [CONTENT_SIZE];
    for (unsigned i = 0; i < FILE_SIZE; i += CONTENT_SIZE) {
        int numBytes = openFile->Read(buffer, CONTENT_SIZE);
        if (numBytes < 10 || strncmp(buffer, CONTENTS, CONTENT_SIZE)) {
            printf("Perf test: unable to read %s\n", FILE_NAME);
            break;
        }
    }

    delete [] buffer;
    delete openFile;
}

#ifdef SYNCH_FSTEST
static void
Writer(void *args){
    if (openFile == nullptr) {
        fprintf(stderr, "Perf test: unable to write file %s\n", FILE_NAME);
        return;
    }

    for(int i=0; i<26; i++){
        openFile->Write(A,1);
    }
}

static void
Reader(void *args){
    if (openFile == nullptr) {
        fprintf(stderr, "Perf test: unable to read file %s\n", FILE_NAME);
        return;
    }

    char buffer[26];
    for(int i=0; i<26; i++){
        openFile->ReadAt(&buffer[i], 1, i);
    }
    
}
#endif

void
PerformanceTest()
{
    #ifdef SYNCH_FSTEST
    fileSystem->Create(FILE_NAME, 0);
    openFile = fileSystem->Open(FILE_NAME);
        Thread *reader1 = new Thread("reader1", true);
        Thread *reader2 = new Thread("reader2", true);
        Thread *writer1 = new Thread("writer1", true);
        Thread *writer2 = new Thread("wirter2", true);
        
        reader1->Fork(Reader, nullptr);
        reader2->Fork(Reader, nullptr);
        writer1->Fork(Writer, nullptr);
        writer2->Fork(Writer, nullptr);
    
        reader1->Join();
        reader2->Join();
        writer1->Join();
        writer2->Join();

    delete openFile;
    #elif DIRECTORY

    printf("Starting directory test:\n");
    OpenFile *openFile;
    if(!fileSystem->Create("dir1",0, true)){
        printf("Create failed.\n");
    } else{
        printf("Created `dir1` succesfully.\n");
    }

    if(!fileSystem->Create("dir1/dir2",0, true)){
        printf("Create failed.\n");
    } else{
        printf("Created `dir1/dir2` succesfully.\n");
    }

    Path *path = new Path("dir1/dir2", false);
    fileSystem->ChangeDirectory(path);

    if(!fileSystem->Create("file1",0, false)){
        printf("Create failed.\n");
    } else{
        printf("Created `file1` succesfully.\n");
    }

    if(!fileSystem->Create("/file2",0, false)){
        printf("Create failed.\n");
    } else{
        printf("Created `file2` succesfully.\n");
    }
    
    // Testing absolute paths.
    OpenFile *file2 = fileSystem->Open("/file2");
    if(file2->Write("Hello World\n", 13) == 13){
        printf("Written 13 bytes succesfully.\n");
    } else{
        printf("Error writing `Hello World` into `file2` succesfully.\n");
    }
    if(file2)
        delete file2;

    // Test remove directory.
    if (!fileSystem->Remove("/dir1")) {
        printf("Deleted failed.\n");
        return;
    } else{
        printf("Deleted `dir1` succesfully.\n");
    }

    
    #else
    printf("Starting file system performance test:\n");
    stats->Print();
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FILE_NAME)) {
        printf("Perf test: unable to remove %s\n", FILE_NAME);
        return;
    }
    stats->Print();
    #endif
}
