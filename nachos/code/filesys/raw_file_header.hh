/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_RAWFILEHEADER__HH
#define NACHOS_FILESYS_RAWFILEHEADER__HH

#include "machine/disk.hh"

static const unsigned NUM_DIRECT
  = (SECTOR_SIZE - 2 * sizeof (int)) / sizeof (int) - 3;
const unsigned NUM_INDIRECT = 32;
static const unsigned FIRST_INDIRECTION = NUM_DIRECT;
static const unsigned SECOND_INDIRECTION = NUM_DIRECT + 1;
const unsigned MAX_FILE_SIZE = NUM_DIRECT * SECTOR_SIZE;

/* Se modifico el FileHeader original de nachos para que se comporte como
  de manera similar a los archivos de Unix, se tienen NUM_DIRECT bloques de acceso directo,
  y luego dos punteros que apuntan a estructuras del tipo IndirectRawFileHeader donde la estructura
  referenciada por el primer puntero son bloques de acceso constante, y los del segundo son punteros a 
  otra estructura de IndirectRawFileHeader que funciona como la apuntada por el primer puntero.

  Logrando asi tener NUM_DIRECT + NUM_INDIRECT + (NUM_INDIRECT ^ 2) bloques para guardar informacion de los archivos.
  
  El tamaño máximo que puede tener un archivo es: (27 + 32 + 32*32) * 128 = 135,375kb
  */

struct RawFileHeader {
    bool isDirectory;
    unsigned numBytes;  ///< Number of bytes in the file.
    unsigned numSectors;  ///< Number of data sectors in the file.
    unsigned dataSectors[NUM_DIRECT + 2];  ///< Disk sector numbers for each data
                                           ///< block in the file.
}; 

struct IndirectRawFileHeader {
    unsigned dataSectors[NUM_INDIRECT];
}; 

#endif

