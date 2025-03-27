/// Routines to manage an open Nachos file.  As in UNIX, a file must be open
/// before we can read or write to it.  Once we are all done, we can close it
/// (in Nachos, by deleting the `OpenFile` data structure).
///
/// Also as in UNIX, for convenience, we keep the file header in memory while
/// the file is open.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "open_file.hh"
#include "file_header.hh"
#include "threads/system.hh"

#include <string.h>


/// Open a Nachos file for reading and writing.  Bring the file header into
/// memory while the file is open.
///
/// * `sector` is the location on disk of the file header for this file.
OpenFile::OpenFile(int sector_, const char* name )
{
    hdr = new FileHeader;
    hdr->FetchFrom(sector_);
    seekPosition = 0;
    sector = sector_;

    files->AddLink(sector, name);
}


/// Close a Nachos file, de-allocating any in-memory data structures.
OpenFile::~OpenFile()
{   
    const char* deleted = files->RemoveLink(sector);
    DEBUG('f', "Unlinking file (sector: %u)\n", sector);
    if(deleted){
        DEBUG('f', "And deleting %s from filesystem\n",deleted);
        fileSystem->Remove(deleted);
        // delete deleted;
    }
    delete hdr;
}

/// Change the current location within the open file -- the point at which
/// the next `Read` or `Write` will start from.
///
/// * `position` is the location within the file for the next `Read`/`Write`.
void
OpenFile::Seek(unsigned position)
{
    seekPosition = position;
}

/// OpenFile::Read/Write
///
/// Read/write a portion of a file, starting from `seekPosition`.  Return the
/// number of bytes actually written or read, and as a side effect, increment
/// the current position within the file.
///
/// Implemented using the more primitive `ReadAt`/`WriteAt`.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.

int
OpenFile::Read(char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    int result = ReadAt(into, numBytes, seekPosition);
    seekPosition += result;
    return result;
}

int
OpenFile::Write(const char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    int result = WriteAt(into, numBytes, seekPosition);
    seekPosition += result;
    return result;
}

/// OpenFile::ReadAt/WriteAt
///
/// Read/write a portion of a file, starting at `position`.  Return the
/// number of bytes actually written or read, but has no side effects (except
/// that `Write` modifies the file, of course).
///
/// There is no guarantee the request starts or ends on an even disk sector
/// boundary; however the disk only knows how to read/write a whole disk
/// sector at a time.  Thus:
///
/// For ReadAt:
///     We read in all of the full or partial sectors that are part of the
///     request, but we only copy the part we are interested in.
/// For WriteAt:
///     We must first read in any sectors that will be partially written, so
///     that we do not overwrite the unmodified portion.  We then copy in the
///     data that will be modified, and write back all the full or partial
///     sectors that are part of the request.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.
/// * `position` is the offset within the file of the first byte to be
///   read/written.

// a) El mismo archivo puede leerse/escribirse con m´as de un hilo concurrentemente.
// Cada hilo abre el archivo de forma separada, lo cual le confiere su propia posici´on
// de “seek” privada dentro del mismo.

// b) Todas las operaciones del sistema de archivos deben ser at´omicas y serializables. Por ejemplo, si un hilo est´a en el medio de una escritura,
//  un hilo que concurrentemente lea el archivo ver´a o bien el cambio completo o ning´un cambio.
// Adem´as, si la operaci´on OpenFile::Write finaliza antes de que empiece la llamada a OpenFile::Read, entonces Read debe reflejar la versi´on modificada del
// archivo.


int
OpenFile::ReadAt(char *into, unsigned numBytes, unsigned position, bool bypass)
{
    // Lector
    // si nadie escribe -> leo normal.
    // si alguien esta escribiendo, espero a que termine para leer el cambio.
    
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    char *buf;

    FileTableEntry *file = files->FindBySector(sector);
    if(!bypass)
        file->RequestRead();
    hdr->FetchFrom(sector);


    if (position >= fileLength){
        if(!bypass)
             file->ReadFree();
        return 0;  // Check request.
    }

    if (position + numBytes > fileLength)
        numBytes = fileLength - position;
    DEBUG('f', "Reading %u bytes at %u, from file (%u) of length %u.\n",
          numBytes, position,sector, fileLength);

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors = 1 + lastSector - firstSector;

    // Read in all the full and partial sectors that we need.
    buf = new char [numSectors * SECTOR_SIZE];
    for (unsigned i = firstSector; i <= lastSector; i++)
        synchDisk->ReadSector(hdr->ByteToSector(i * SECTOR_SIZE),
                              &buf[(i - firstSector) * SECTOR_SIZE]);

    // Copy the part we want.
    memcpy(into, &buf[position - firstSector * SECTOR_SIZE], numBytes);
    delete [] buf;
    if(!bypass){
        file->ReadFree();
    }
    return numBytes;
}

int
OpenFile::WriteAt(const char *from, unsigned numBytes, unsigned position)
{
    // Escritor
    // si nadie escribe -> escribir.
    // si alguien esta escribiendo, espero a que termine para leer el cambio.

    ASSERT(from != nullptr);
    ASSERT(numBytes > 0);
    
    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    bool firstAligned, lastAligned;
    char *buf;

    FileTableEntry *file = files->FindBySector(sector);
    file->RequestWrite();
    hdr->FetchFrom(sector);

    if (position + numBytes > fileLength) {
        unsigned sizeToExpand = numBytes - (fileLength - position);
        DEBUG('f',"File (%u), needs to expand %u bytes.\n",sector,sizeToExpand);
        
        if(hdr->FileExpand(sizeToExpand, sector)){
            fileLength = hdr->FileLength();
        }
        else return 0;
    }

    DEBUG('f', "Writing %u bytes at %u, from file (%u) of length %u.\n",
        numBytes, position, sector, fileLength);

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector  = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors  = 1 + lastSector - firstSector;

    buf = new char [numSectors * SECTOR_SIZE];

    firstAligned = position == firstSector * SECTOR_SIZE;
    lastAligned  = position + numBytes == (lastSector + 1) * SECTOR_SIZE;

    // Read in first and last sector, if they are to be partially modified.
    // If needed, bypass the lock to read.
    if (!firstAligned)
        ReadAt(buf, SECTOR_SIZE, firstSector * SECTOR_SIZE, true);
    if (!lastAligned && (firstSector != lastSector || firstAligned))
        ReadAt(&buf[(lastSector - firstSector) * SECTOR_SIZE],
               SECTOR_SIZE, lastSector * SECTOR_SIZE, true);

    // Copy in the bytes we want to change.
    memcpy(&buf[position - firstSector * SECTOR_SIZE], from, numBytes);

    // Write modified sectors back.
    for (unsigned i = firstSector; i <= lastSector; i++)
        synchDisk->WriteSector(hdr->ByteToSector(i * SECTOR_SIZE),
                               &buf[(i - firstSector) * SECTOR_SIZE]);
    
    file->WriterFree();
    delete [] buf;
    return numBytes;
}

/// Return the number of bytes in the file.
unsigned
OpenFile::Length() const
{
    return hdr->FileLength();
}

unsigned
OpenFile::GetSector(){
    return sector;
}
