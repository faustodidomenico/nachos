/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>


/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.

/* Esta funcion se utiliza en la inicializacion de un archivo, o la expansion de uno ya existente (initialSector != 0).
    Luego dependiendo de la cantidad de bloques que tiene reservados en disco el archivo, reserva los bloques directos, de primer indireccion
    o de segunda indireccion (ademas de los bloques reservados para las estructuras).
*/
bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize, unsigned initialSector)
{
    DEBUG('g',"Allocating %d bytes.\n",fileSize);
    unsigned allocated = initialSector;
    ASSERT(freeMap != nullptr);

    raw.numBytes = fileSize;
    raw.numSectors = DivRoundUp(fileSize, SECTOR_SIZE);
    if (freeMap->CountClear() < raw.numSectors)
        return false;  // Not enough space.

    // Alojar los bloques directos.
    DEBUG('g',"Allocating direct blocks .\n");
    for ( ; allocated < NUM_DIRECT && allocated < raw.numSectors; allocated++){
        raw.dataSectors[allocated] = freeMap->Find();
        DEBUG('j',"Found empty space for sector0 %u: %u \n",allocated, raw.dataSectors[allocated]);
    }

    if(raw.numSectors >= (NUM_DIRECT) && allocated < raw.numSectors){
    // Alojar los bloques de primer indireccion.
        DEBUG('g', "Allocating first indrection.\n");

        IndirectRawFileHeader indirectRawFileHeader;
        // Hay que contemplar este caso ya que habra veces que 
        // queremos expandir pero de igual forma hay que asignar
        // espacio para los punteros de primer o segunda indireccion.
        if(initialSector <= FIRST_INDIRECTION){
            raw.dataSectors[FIRST_INDIRECTION] = freeMap->Find();
        } else{
            synchDisk->ReadSector(raw.dataSectors[FIRST_INDIRECTION], (char *) &indirectRawFileHeader);
        }

        for ( ; allocated < NUM_DIRECT + NUM_INDIRECT && allocated < raw.numSectors; allocated++){
            indirectRawFileHeader.dataSectors[allocated - (NUM_DIRECT)] = freeMap->Find();
            DEBUG('j',"Found empty space for sector1 %u: %u \n",allocated, indirectRawFileHeader.dataSectors[allocated - (NUM_DIRECT)]);
        }
        
        synchDisk->WriteSector(raw.dataSectors[NUM_DIRECT], (char *) &indirectRawFileHeader);
    }
    if(raw.numSectors >= NUM_DIRECT + NUM_INDIRECT && allocated < raw.numSectors){
    // Alojar los bloques de segunda indireccion
        DEBUG('i', "Allocating second indrection.\n");
        IndirectRawFileHeader indirectRawFileHeaderBlocks;
        
        if(initialSector <= NUM_DIRECT + NUM_INDIRECT){
            raw.dataSectors[SECOND_INDIRECTION] = freeMap->Find();
        } else{
            synchDisk->ReadSector(raw.dataSectors[SECOND_INDIRECTION], (char *) &indirectRawFileHeaderBlocks);
        }
        
        unsigned remaining = raw.numSectors - allocated;
        remaining = DivRoundUp(remaining,NUM_INDIRECT);
        IndirectRawFileHeader indirectRawFileHeader;
        for (unsigned i = 0; i < remaining; i++)
        {
            if (initialSector <= NUM_DIRECT + NUM_INDIRECT + NUM_INDIRECT * i)
                indirectRawFileHeaderBlocks.dataSectors[i] = freeMap->Find();
            else 
                synchDisk->ReadSector(indirectRawFileHeaderBlocks.dataSectors[i], (char *) &indirectRawFileHeader);

            unsigned j = 0;
            
            if(initialSector >= NUM_DIRECT + NUM_INDIRECT + NUM_INDIRECT * i && initialSector < NUM_DIRECT + NUM_INDIRECT + NUM_INDIRECT * (i+1)){
                j = NUM_INDIRECT - (NUM_DIRECT + NUM_INDIRECT + NUM_INDIRECT * (i+1) - initialSector);
            }

            for( ; j < NUM_INDIRECT && allocated < raw.numSectors; j++, allocated++){
                indirectRawFileHeader.dataSectors[j] = freeMap->Find();
                DEBUG('j',"Found empty space for sector2 %u: %u \n",allocated, indirectRawFileHeader.dataSectors[j]);
            }

            synchDisk->WriteSector(indirectRawFileHeaderBlocks.dataSectors[i], (char *) &indirectRawFileHeader);
        }
        synchDisk->WriteSector(raw.dataSectors[SECOND_INDIRECTION], (char *) &indirectRawFileHeaderBlocks);        
    }

    return allocated == raw.numSectors;
}

/* De manera inversa al Allocate, libera los espacios reservados tanto de bloques directos, estructuras y bloques indirectos. */

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors
void
FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);

    unsigned i = 0;

    for ( ; i < NUM_DIRECT && i < raw.numSectors; i++) {
        ASSERT(freeMap->Test(raw.dataSectors[i]));  // ought to be marked!
        freeMap->Clear(raw.dataSectors[i]);
    }

    if(raw.numSectors >= (NUM_DIRECT)){
        IndirectRawFileHeader indirectRawFileHeader;
        synchDisk -> ReadSector(raw.dataSectors[NUM_DIRECT],(char *)&indirectRawFileHeader);

        for (; i < NUM_DIRECT + NUM_INDIRECT && i < raw.numSectors; i++){
            ASSERT(freeMap->Test(indirectRawFileHeader.dataSectors[i - NUM_DIRECT]));  // ought to be marked!
            freeMap->Clear(indirectRawFileHeader.dataSectors[i - NUM_DIRECT]);
        }     
    }
    if(raw.numSectors >= NUM_DIRECT + NUM_INDIRECT){
    // Alojar los bloques de segunda indireccion
        
        // Block of blocks.
        IndirectRawFileHeader indirectRawFileHeaderBlocks;

        unsigned remaining = raw.numSectors - i;
        remaining = DivRoundUp(remaining,NUM_INDIRECT);
        IndirectRawFileHeader indirectRawFileHeader;
        for (unsigned k = 0; k < remaining; k++)
        {
            for(unsigned j = 0; j < NUM_INDIRECT && i < raw.numSectors; j++, i++){
                freeMap->Clear(indirectRawFileHeader.dataSectors[j]);
            }

            freeMap->Clear(indirectRawFileHeaderBlocks.dataSectors[k]);
        }
    }
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
    synchDisk->ReadSector(sector, (char *) &raw);
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
    synchDisk->WriteSector(sector, (char *) &raw);
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.

/* Dado el offset (cursor de un archivo) calcula el sector del disco donde se encuentra. 
    Accediendo los punteros de indireccion que sean necesarios o no, si se encuentra en un bloque directo. */

unsigned
FileHeader::ByteToSector(unsigned offset)
{
    DEBUG('g', "Translating offset %d of file in bytes to disk sector.",offset);
    // Aca hay que cambiar para obtener el sector
    // correspondiente.
    unsigned sector = DivRoundDown(offset,SECTOR_SIZE);
    if(sector < NUM_DIRECT){
        DEBUG('g', "SECTOR: %d",sector);
        DEBUG('g', "RET (direct): %d\n", raw.dataSectors[sector]);
        return raw.dataSectors[sector];
    } 
    else if(sector < (NUM_DIRECT + NUM_INDIRECT)){
        offset = sector - (NUM_DIRECT);
        
        IndirectRawFileHeader indirectRawFileHeader;
        synchDisk -> ReadSector(raw.dataSectors[FIRST_INDIRECTION],(char *)&indirectRawFileHeader);
        DEBUG('g', "RET (1st indirect): %d\n", raw.dataSectors[sector]);
        return indirectRawFileHeader.dataSectors[offset];
    } 
    // Sector >= 60
    else if(sector < ((NUM_DIRECT) + NUM_INDIRECT) + (NUM_INDIRECT * NUM_INDIRECT)){
        unsigned block = (sector - (NUM_DIRECT + NUM_INDIRECT)) / NUM_INDIRECT;
        IndirectRawFileHeader indirectRawFileHeader;
        offset = (sector - (NUM_DIRECT + NUM_INDIRECT)) % NUM_INDIRECT;
        synchDisk -> ReadSector(raw.dataSectors[SECOND_INDIRECTION],(char *)&indirectRawFileHeader);
        DEBUG('g', "Read 2nd indirect block of blocks xd \n");
        IndirectRawFileHeader indirectRawFileHeader2;
        synchDisk -> ReadSector(indirectRawFileHeader.dataSectors[block],(char *)&indirectRawFileHeader2);
        DEBUG('g', "RET (2nd indirect): %d\n", indirectRawFileHeader2.dataSectors[offset]);
        return indirectRawFileHeader2.dataSectors[offset];

    } else{
        // Usamos el 0 como representacion del error,
        // ya que por convencion en el sector 0 se encuentra
        // el bitmap del disco.
        return 0;
    }
}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    return raw.numBytes;
}

/* Reutiliza la funcion Allocate para expandir un archivo. */
bool
FileHeader::FileExpand(unsigned sizeToExpand, unsigned sector){

    OpenFile* freeMapFile = new OpenFile(0);
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    bool success = Allocate(freeMap, sizeToExpand + raw.numBytes, raw.numSectors);
    if(success){
        freeMap->WriteBack(freeMapFile);
        WriteBack(sector);
    }

    delete freeMapFile;

    return success;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
    // // Ver que hay que modificar.
    char *data = new char [SECTOR_SIZE];

    if (title == nullptr)
        printf("File header:\n");
    else
        printf("%s file header:\n", title);

    printf("    size: %u bytes\n"
           "    block indexes: ",
           raw.numBytes);

    for (unsigned i = 0; i < raw.numSectors ; i++){
        printf("%u ", ByteToSector(i * SECTOR_SIZE));
    }
    printf("\n");

    for (unsigned i = 0, k = 0; i < raw.numSectors; i++) {
        printf("    contents of block %u:\n", raw.dataSectors[i]);
        synchDisk->ReadSector(ByteToSector(i * SECTOR_SIZE), data);
        for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++) {
            if (isprint(data[j]))
                printf("%c", data[j]);
            else
                printf("\\%X", (unsigned char) data[j]);
        }
        printf("\n");
    }
    delete [] data;
}

const RawFileHeader *
FileHeader::GetRaw() const
{
    return &raw;
}

void
FileHeader::SetIsDirectory(bool isDirectory){
    raw.isDirectory = isDirectory;
}

bool
FileHeader::IsDirectory(){
    return raw.isDirectory;
}