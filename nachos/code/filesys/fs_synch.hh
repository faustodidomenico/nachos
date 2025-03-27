#include "lib/list.hh"
#include "threads/synch.hh"

class FileTableEntry{

public:
    FileTableEntry(unsigned sect, const char* name_);

    ~FileTableEntry();

    // Number of process that have this file open.
    unsigned open;

    // First sector used by file.
    unsigned sector;
    
    // Flag to determine if this file should be deleted.
    bool deleted;

    // Filename to be deleted
    const char *name;

    // Aquire lock and increment readers.
    void RequestRead();

    //  Decrement readers an
    void ReadFree();

    void RequestWrite();

    void WriterFree();


private:
    Lock *lock;
    Condition *cond;
    unsigned reading;
    bool writing;    
};

class FileTable {

public:

    FileTable();

    ~FileTable();

    // Creates link to the file.
    void AddLink(unsigned sector, const char* name);

    // Remove link to the file, if it's the last one. 
    const char* RemoveLink(unsigned sector);

    FileTableEntry* FindBySector(unsigned sector);
    
    unsigned GetCount();

private:
    
    unsigned count;
    // List as abstraction of file table.
    List<FileTableEntry*> *table;

    // Method to get table entries.
};
