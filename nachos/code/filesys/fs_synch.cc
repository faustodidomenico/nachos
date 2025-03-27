#include "fs_synch.hh"

FileTableEntry::FileTableEntry(unsigned sect, const char* name_) {    
    sector = sect;
    open = 1;
    deleted = false;
    name = name_;

    // Producer/Consumer logic. Based on:
    // http://pages.cs.wisc.edu/~jacobson/cs537/S2012/handouts/lecture-cv.pdf
    lock = new Lock(name);
    cond = new Condition(name, lock);
    reading = 0;
    writing = false;
}

FileTableEntry::~FileTableEntry() {}

void 
FileTableEntry::RequestRead(){
    lock->Acquire();
    while(writing) cond->Wait();
    reading++;
    lock->Release();
}

void
FileTableEntry::ReadFree(){
    lock->Acquire();
    reading--;
    if (!reading) cond->Broadcast();
    lock->Release();
}

void
FileTableEntry::RequestWrite(){
    
    lock->Acquire();
    while (writing || reading > 0) cond->Wait();
    DEBUG('b',"Setting writing to TRUE.\n");
    writing = true;
    lock->Release();
}


void 
FileTableEntry::WriterFree(){
    lock->Acquire();
    DEBUG('b', "Setting writing to false.\n");
    writing = false;
    cond->Broadcast();
    lock->Release();
}


FileTable::FileTable(){
    table = new List<FileTableEntry*>();
    count = 0;
}

FileTable::~FileTable(){
    delete table;
}

unsigned
FileTable::GetCount(){
    return count;
}

void
FileTable::AddLink(unsigned sector, const char* name){
    
    FileTableEntry* file = FindBySector(sector);
    
    if(!file){
      file = new FileTableEntry(sector, name);
      table->SortedInsert(file,sector);
      count++;
    } else{
        file->open++;
    }
    DEBUG('f', "Number of links of file (sector %d) : %u \n",sector, file->open);
}

const char*
FileTable::RemoveLink(unsigned sector){
    
    FileTableEntry* file = FindBySector(sector);
    
    if(file){
        file->open--;
        if(!file->open){
            table->Remove(file);
            count--;
            if(file->deleted){
                return file->name;
            } 
        }
    }
    return nullptr;
}

FileTableEntry*
FileTable::FindBySector(unsigned sector){
    return table->FindByKey(sector);
}


