/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "threads/system.hh"
#include "lib/bitmap.hh"
#include <string.h>
#include <stdio.h>
	
uint32_t
min(int a, int b){
  return (a<b?a:b);
}

uint32_t
max(int a, int b){
  return (a>b?a:b);
}

/* Dada una direccion virtual retorna la direccion fisica correspondiente. */
unsigned
VirtualToPhysical(unsigned virtualAddr, TranslationEntry *pageTable){
    unsigned virtualPage = DivRoundDown(virtualAddr, PAGE_SIZE);
    unsigned offset = virtualAddr % PAGE_SIZE;
    unsigned frame = pageTable[virtualPage].physicalPage;
    return frame * PAGE_SIZE + offset;
}

/* Inicializa un espacio de direcciones para un proceso. 
   - Inicializa tabla de paginas
   - Segun si:
        - No esta definida VMEM: realiza una asignacion 1 a 1 (paginaVirtual, paginaFisica). Y carga todo el ejecutable en memoria.
        - Si esta definida VMEM, se utiliza paginacion por demanda por lo que todas las paginas del nuevo proceso
            comienzan apuntando al número de paginas + 1 e invalidas. Además crea el archivo de swap asociado a este proceso. */
AddressSpace::AddressSpace(OpenFile *executable_file, int id)
{
    ASSERT(executable_file != nullptr);

    exe = new Executable(executable_file);
    ASSERT(exe->CheckMagic());

    asid = id;

    // How big is address space?
    unsigned size = exe->GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;

    // Check we are not trying to run anything too big -- at least until we
    // have virtual memory.

    #ifndef VMEM
    ASSERT(numPages <= freeMemMap->CountClear());  
    #else
    DEBUG('a', "Initializing address space swap file. ASID: %d\n",asid);
    char swapFileName[16];
    snprintf(swapFileName, 16, "SWAP.%d", asid);
    
    fileSystem->Create(swapFileName, asid);
    swapFile = fileSystem->Open(swapFileName);
    #endif  

    DEBUG('a', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.
    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        #ifndef USE_TLB
        pageTable[i].physicalPage = freeMemMap->Find();
        pageTable[i].valid        = true;
        #else
        pageTable[i].physicalPage = numPages + 1;
        pageTable[i].valid        = false;
        #endif
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
        // If the code segment was entirely on a separate page, we could
        // set its pages to be read-only.
    }

    DEBUG('a', "Page table initialized succesfully.\n");
    
    // Then, copy in the code and data segments into memory.
    #ifndef VMEM
    char *mainMemory = machine->GetMMU()->mainMemory;  
    uint32_t codeSize = exe->GetCodeSize();
    uint32_t initDataSize = exe->GetInitDataSize();
    if (codeSize > 0) {
        uint32_t virtualAddr = exe->GetCodeAddr();
        DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
              virtualAddr, codeSize);

        unsigned realAddr;
        for(unsigned codeByte = 0; codeByte < codeSize; codeByte++){
          realAddr = VirtualToPhysical(virtualAddr+codeByte,pageTable);
          exe->ReadCodeBlock(&(mainMemory[realAddr]), 1, codeByte);
        }
    }
    if (initDataSize > 0) {
        uint32_t virtualAddr = exe->GetInitDataAddr();
        DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",
              virtualAddr, initDataSize);
        
        unsigned realAddr;
        for(unsigned codeByte = 0; codeByte < initDataSize; codeByte++){
          realAddr = VirtualToPhysical(virtualAddr+codeByte,pageTable);
          exe->ReadDataBlock(&(mainMemory[realAddr]), 1, codeByte);
        }
    }
    #endif
}


/* Libera la tabla de páginas y los espacios reservados del freeMemMap que refieren a las paginas liberadas. */ 

/// Deallocate an address space.
/// Clear freeMemMap.
AddressSpace::~AddressSpace()
{
    for (unsigned i = 0; i < numPages; i++) 
        if (pageTable[i].valid)
            freeMemMap->Clear(pageTable[i].physicalPage);
        
    delete [] pageTable;
    delete exe;
}


/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++)
        machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!

    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}


/* Al realizarse un cambio de contexto, se copia el estado de las paginas de la tlb validas a la tabla de paginas del proceso. */
/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
void
AddressSpace::SaveState()
{
  #ifdef USE_TLB
   TranslationEntry tlbEntry;
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        tlbEntry = machine->GetMMU()->tlb[i];
        if (tlbEntry.valid) {
            pageTable[tlbEntry.virtualPage] = tlbEntry;
        }
   }
  #endif
}

/* Cuando un proceso retorna de un cambio de contexto, las paginas de la TLB le corresponden al proceso que se ejecutaba anteriormente, por
   lo que se deben invalidar las paginas de la TLB, resultando que se vuelvan a cargar de ser necesario. */

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
    #ifdef USE_TLB
    for(unsigned int i=0; i<TLB_SIZE; i++) machine->GetMMU()->tlb[i].valid = false;    
    #else
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
    #endif
}

#ifdef USE_TLB
/* Esta operacion es utilizada desde el manejador de fallos de paginas y dada una pagina virtual, devuelve la pagina fisica asociada al numero de
   pagina virtual.
   Si no hay mas espacio fisico, remueve una pagina utilizando `ClearPage` el cual dependiendo del algoritmo configurado selecciona una pagina victima.
    
   Luego dependiendo de si la pagina se encuentra en SWAP o no, la carga y la retorna.
*/
TranslationEntry 
AddressSpace::LoadPage(unsigned vpn){

    // VPN correspone a la direccion virtual del proceso.
    unsigned codeSize = exe->GetCodeSize();
    unsigned dataSize = exe->GetInitDataSize();
    uint32_t codeAddr = exe->GetCodeAddr();
    uint32_t dataAddr = exe->GetInitDataAddr();

    int sizeToRead, offset, remaining = PAGE_SIZE;
    unsigned virtualAddr = vpn * PAGE_SIZE;

         
    int ppn = freeMemMap->Find(this);
    if(ppn < 0){
        freeMemMap->ClearPage(this);
        return LoadPage(vpn);
    } else{
        if(vpn < 0){
            DEBUG('h',"Error: VPN < 0, finishing current thread.\n");
            currentThread->Finish();
        } else {
                char *mainMemory = machine->GetMMU()->mainMemory;  
                unsigned realAddr = ppn * PAGE_SIZE;
            
                SaveState();
            
            if(pageTable[vpn].dirty){
                DEBUG('k',"Loading page from SWAP.\n");
                swapFile->ReadAt(&(mainMemory[realAddr]), PAGE_SIZE, vpn * PAGE_SIZE);
            } else {
                // Code segment
                if(virtualAddr < (codeSize+codeAddr)){
                  DEBUG('k',"Code Page\n");
                  sizeToRead = min(remaining,(int)((codeSize + codeAddr) - virtualAddr));
                  offset = max(0,virtualAddr - codeAddr);
                  exe->ReadCodeBlock(&mainMemory[realAddr], sizeToRead, offset);
                  remaining -= sizeToRead;
                  virtualAddr += sizeToRead;
                }

                // Data segment
                if((virtualAddr < dataSize+dataAddr) && (remaining>0) && (dataSize)){
                  DEBUG('k',"Data Page\n");
                  sizeToRead = min(remaining, (dataSize + dataAddr) - virtualAddr);
                  offset = max(0,virtualAddr - dataAddr);
                  exe->ReadDataBlock(&mainMemory[realAddr + (PAGE_SIZE - remaining)],sizeToRead,offset);
                  remaining -= sizeToRead;
                  virtualAddr += sizeToRead;
                }

                // Stack segment
                if(remaining>0){
                  DEBUG('k',"Stack Page\n");
                  memset(mainMemory + ((ppn+1) * PAGE_SIZE - remaining), 0, remaining);
                }
            }
                pageTable[vpn].virtualPage  = vpn;
                pageTable[vpn].physicalPage = ppn;
                pageTable[vpn].valid = true;
                DEBUG('k',"Page loaded succesfully. VPN: %d, PPN: %d\n",vpn,ppn);
        }
    }
    return pageTable[vpn];
}

/* Dada una pagina fisica, busca si existe la pagina virtual. */
int
AddressSpace::FindVirtualPage(unsigned ppn){
    int vpn = -1;
    // Look for the virtual page that matches this physical page.
    for(unsigned i = 0; i < numPages; i++ ){
        // DEBUG('k',"i: %d, PP: %d, VALID: %d PPN: %d\n",i,pageTable[i].physicalPage, pageTable[i].valid, ppn);
        if(pageTable[i].physicalPage == ppn && pageTable[i].valid ){
        vpn = i;
        break;
        }
    }
    return vpn;
}

/* Luego de que el `ClearPage` seleccione una pagina victima, se remueve utilizando este metodo, que dependiendo de si la pagina 
se encontraba sucia, se guarda en swap. Luego se invalidan las paginas de la tlb y se actualiza el estado.  */
void
AddressSpace::RemovePage(unsigned ppn){
    unsigned frame = ppn * PAGE_SIZE;
    char *mainMemory = machine->GetMMU()->mainMemory;
    
    int vpn = FindVirtualPage(ppn);
    if(vpn < 0){
        DEBUG('k',"Physical page (%d) doesn't match with any Virtual page.\n", ppn);
        currentThread->Finish();
    } else{
        
        if(pageTable[vpn].dirty){
            // Save it at SWAP.
            DEBUG('s',"Saving page at SWAP.%d file.\n",currentThread->space->asid);
            //pageState[vpn] = 1;
            swapFile->WriteAt(&mainMemory[frame], PAGE_SIZE, vpn * PAGE_SIZE);
            memset(&mainMemory[frame], 0, PAGE_SIZE);
        }

        TranslationEntry *tlb = machine -> GetMMU() -> tlb;
        // Invalidamos las paginas de la tlb y ademas le copiamos el estado.
        pageTable[vpn].valid = false;
        pageTable[vpn].physicalPage = numPages + 1;
        for(unsigned i = 0; i < TLB_SIZE; ++i)
            if(tlb[i].physicalPage == ppn && tlb[i].valid){

                // Update the tablePage entry with actual tlb entry.
                pageTable[vpn].virtualPage = tlb[i].virtualPage;
                pageTable[vpn].dirty = tlb[i].dirty;
                pageTable[vpn].readOnly = tlb[i].readOnly;
                pageTable[vpn].use = tlb[i].use;

                tlb[i].valid = false;
                tlb[i].physicalPage = numPages+1;
                break;
            }   
        if(currentThread->space == this)            
		    SaveState();
        freeMemMap->Clear(ppn);
    }
  }   

TranslationEntry * 
AddressSpace::GetPageTable(){
    return pageTable;
}

#endif

