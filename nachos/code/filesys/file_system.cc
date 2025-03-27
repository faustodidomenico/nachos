/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "directory.hh"
#include "directory_entry.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "machine/disk.hh"
#include "threads/system.hh"
#include <stdio.h>
#include <string.h>

int FindByPath(Path *path, OpenFile *dirFile);
int ChangeDirectory(Path *path);
/// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;

/// Initial file sizes for the bitmap and directory; until the file system
/// supports extensible files, the directory size sets the maximum number of
/// files that can be loaded onto the disk.
static const unsigned FREE_MAP_FILE_SIZE = NUM_SECTORS / BITS_IN_BYTE;
static const unsigned NUM_DIR_ENTRIES = 1;
static const unsigned DIRECTORY_FILE_SIZE = sizeof (DirectoryEntry)
                                            * NUM_DIR_ENTRIES;

/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory(NUM_DIR_ENTRIES);
        FileHeader *mapH    = new FileHeader;
        FileHeader *dirH    = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile   = new OpenFile(FREE_MAP_SECTOR, "FreeMap");
        directoryFile = new OpenFile(DIRECTORY_SECTOR, "Directory");
        
        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Files table length: %u\n", files->GetCount());

        DEBUG('f', "Writing bitmap back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        DEBUG('f', "Writing directory back to disk.\n");
        dir->WriteBack(directoryFile);

        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();

            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR, "FreeMap");
        directoryFile = new OpenFile(DIRECTORY_SECTOR, "Directory");
    }
}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// Create the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::Create(const char *name, unsigned initialSize, bool isDirectory)
{
    ASSERT(name != nullptr);

    DEBUG('f', "Creating file %s, size %u\n", name, initialSize);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile *dirTempFile = directoryFile;
    #ifdef DIRECTORY
    Path* path = new Path(name, true);
    int oldDirSector = currentThread->GetDirSector();
    if(path->Length() > 0){
        oldDirSector = ChangeDirectory(path);
    }

    if(oldDirSector == -1){
        // DEBUG!
        return false;
    }
        dirTempFile = new OpenFile(currentThread->GetDirSector(), "TempDirectory");
        dir->FetchFrom(dirTempFile);    
        name = path->GetFileName();
    #else
        dir->FetchFrom(dirTempFile);    
    #endif
    
    bool success;
    if (dir->Find(name) != -1)
        success = false;  // File is already in directory.
    else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1)
            success = false;  // No free block for file header.
        else if (!dir->Add(name, sector))
            success = false;  // No space in directory.
        else {
            
            FileHeader *h = new FileHeader;
            h->SetIsDirectory(isDirectory);
            success = h->Allocate(freeMap, initialSize);
            DEBUG('f', "Result of file allocation %s \n", success ? "true" : "false");

              // Fails if no space on disk for data.
            if (success) {
                // Everything worked, flush all changes back to disk.
                h->WriteBack(sector);
                dir->WriteBack(dirTempFile);
                freeMap->WriteBack(freeMapFile);
                if (isDirectory) {
                    Directory* newDirectory = new Directory(NUM_DIR_ENTRIES);
                    OpenFile* newDirectoryFile = new OpenFile(sector, "TempNewDirectory");
                    newDirectory->WriteBack(newDirectoryFile);
                    // delete newDirectory;
                    delete newDirectoryFile;
                }
            }
            delete h;
        }
        delete freeMap;
    }

    #ifdef DIRECTORY
    if(oldDirSector != -1){
        currentThread->SetDirSector(oldDirSector);
        delete dirTempFile;
    }
    #endif

    delete dir;
    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *name)
{
    ASSERT(name != nullptr);

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile  *openFile = nullptr;
    OpenFile *dirTempFile = directoryFile;

    DEBUG('f', "Opening file %s\n", name);

    #ifdef DIRECTORY
    Path* path = new Path(name, true);
    int oldDirSector = currentThread->GetDirSector();
    if(path->Length() > 0){
        oldDirSector = ChangeDirectory(path);
    }

    if(oldDirSector == -1){
        // DEBUG!
        return nullptr;
    }
        dirTempFile = new OpenFile(currentThread->GetDirSector(), "DirectoryTempFile");
        dir->FetchFrom(dirTempFile);    
        name = path->GetFileName();
    #else
        dir->FetchFrom(dirTempFile);    
    #endif

    int sector = dir->Find(name);
    DEBUG('f', "File with name %s first sector is located at: %d\n", name, sector);
    if (sector >= 0){
        // Validamos si se puede abrir el archivo
        FileTableEntry* file = files->FindBySector(sector);
        if(!file || !file->deleted){
            openFile = new OpenFile(sector, name);  // `name` was found in directory.
        } else{
            DEBUG('f', "Removed file can't be opened. %s\n", name);
        }
    }
    
    #ifdef DIRECTORY
    if(oldDirSector != -1){
        currentThread->SetDirSector(oldDirSector);
        delete dirTempFile;
    }
    #endif
    delete dir;
    return openFile;  // Return null if not found.
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *name)
{
    ASSERT(name != nullptr);

    DEBUG('f', "Removing file with name: %s\n", name);

    OpenFile *dirTempFile = directoryFile;
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    bool success = false;
    
    #ifdef DIRECTORY
    Path* path = new Path(name, true);
    int oldDirSector = currentThread->GetDirSector();
    if(path->Length() > 0){
        oldDirSector = ChangeDirectory(path);
    }

    if(oldDirSector == -1){
        // DEBUG!
        return false;
    }
        dirTempFile = new OpenFile(currentThread->GetDirSector(), "DirTempFile");
        dir->FetchFrom(dirTempFile);    
        name = path->GetFileName();
    #else
        dir->FetchFrom(directoryFile);    
    #endif

    int sector = dir->Find(name);
    if (sector == -1) {
       delete dir;
       return false;  // file not found
    }

    FileTableEntry* fileEntry = files->FindBySector(sector);
    if(!fileEntry || !fileEntry->open){
        FileHeader *fileH = new FileHeader;
        fileH->FetchFrom(sector);
        #ifdef DIRECTORY
        if(fileH->IsDirectory()){
                OpenFile *tempDirFile = new OpenFile(sector,"TempDirRM");
                Directory *tempDir = new Directory(NUM_DIR_ENTRIES);
                tempDir->FetchFrom(tempDirFile);

                const RawDirectory *rawDir = tempDir->GetRaw();
                
                for(int i=0; i < rawDir->tableSize; i++){
                    if(rawDir->table[i].inUse){
                        char* temp = path->AppendToRaw(rawDir->table[i].name);
                        Remove(temp);
                    }
                }
                tempDir->WriteBack(tempDirFile);
                // delete tempDir;
           
        }
        #endif

        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);

        fileH->Deallocate(freeMap);  // Remove data blocks.
        freeMap->Clear(sector);      // Remove header block.
        dir->Remove(name);
    
        freeMap->WriteBack(freeMapFile);  // Flush to disk.
        #ifdef DIRECTORY
        dir->WriteBack(dirTempFile);    // Flush to disk.
        #else
        dir->WriteBack(directoryFile);
        #endif
        delete fileH;
        delete dir;
        delete freeMap;
        success = true;
        
    } else {
        fileEntry->deleted = true;
    }

    #ifdef DIRECTORY
    if(oldDirSector != -1){
        currentThread->SetDirSector(oldDirSector);
        delete dirTempFile;
    }
    #endif
    
    // delete fileEntry;

    return success;
}

/// List all the files in the file system directory.
void
FileSystem::List(char* path_)
{
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile *dirTempFile = directoryFile;
    
    if(path_ != nullptr){
        #ifdef DIRECTORY
        Path *path = new Path(path_);
        int sector = -1;
        if(path->IsRelative()){
            OpenFile *currentDir = new OpenFile(currentThread->GetDirSector(), "CurrentDirectoryTemp");
            sector = FindByPath(path,currentDir);
        } else{
            sector = FindByPath(path,dirTempFile);
        }

        if(sector == -1){
            DEBUG('f',"Invalid path.\n");
            return;
        } else{
            OpenFile *dirFileTemp = new OpenFile(sector, "DirTemp");
            dir->FetchFrom(dirFileTemp);    
            dir->List();
            if(dirFileTemp) delete dirFileTemp;
            delete dir;
        }
        #endif
    } else{

        dir->FetchFrom(dirTempFile);
        dir->List();
        delete dir;
    }
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value)
        DEBUG('f', message);
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    bool error = false;

    error |= CheckForError(sector < NUM_SECTORS, "Sector number too big.\n");
    error |= CheckForError(AddToShadowBitmap(sector, shadowMap),
                           "Sector number already used.\n");
    return error;
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "Sector count not compatible with file size.\n");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
		           "Too many blocks.\n");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "Inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "Bad bitmap header: wrong file size.\n");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "Bad bitmap header: wrong number of sectors.\n");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check succeeded.\n"
                     : "Filesystem check failed.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitH    = new FileHeader;
    FileHeader *dirH    = new FileHeader;
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory(NUM_DIR_ENTRIES);

    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    dir->FetchFrom(directoryFile);
    dir->Print();
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}

#ifdef DIRECTORY
// void
// FileSystem::MakeDirectory(char* path){

//     // Actual directory.
//     Directory* dir = new Directory(10);
//     dir->FetchFrom(directoryFile);
    
//     // actual = /home/federico/so2
//     // path =   /home/federico/so2/
//     // nachos
// }
int
FileSystem::ChangeDirectory(Path *path){
    
    
    int sector = 0;
    if(path->IsRelative()){
        OpenFile *temp = new OpenFile(currentThread->GetDirSector(), "DirTemp1");
        sector = FindByPath(path, temp);
        if (temp) delete temp;
    } else{
        // Open root dirFile
        sector = FindByPath(path, directoryFile);
    }

    if(sector != -1){
        int oldDirSector = currentThread->GetDirSector();
        currentThread->SetDirSector(sector);
        return oldDirSector;
    } else {
        DEBUG('e',"Error: Couldn't change directory. Invalid path.\n");
        return -1;
    }
}

// Return the sector of the deepest directory of the path.
int
FindByPath(Path *path, OpenFile *dirFile){
    Directory *tempDir = new Directory(10);
    tempDir->FetchFrom(dirFile);
    int sector = -1;
    
    List<char*> dirPath = path->GetPath();

    if(!path->IsRelative()){
        if(path->Length() == 1) 
            return 1;
        else
            dirPath.Pop();
    }
    
    while(!dirPath.IsEmpty()){
        
        sector = tempDir->Find(dirPath.Pop());

        // Invalid path.
        if(sector == -1){
            return -1;
        }
        
        OpenFile *tempDirFile = new OpenFile(sector, "TempDirFile1");
        tempDir->FetchFrom(tempDirFile);
        if (tempDirFile) delete tempDirFile;

    }
    

    return sector;
}

#endif