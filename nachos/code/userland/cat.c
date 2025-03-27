// CAT utilitary program. 
#include "syscall.h"

int main(int argc, char** argv){

    if(argc != 2){
        Write("Arguments error\n", 17, CONSOLE_OUTPUT);
        Exit(-1);
    }

    OpenFileId o = Open(argv[1]);
    if(o < 2){
        Write("Failed opening file. \n", 24, CONSOLE_OUTPUT);
        Exit(-1);
    }
    char c[1];
    
    while(Read(c, 1, o, -1) > 0) Write(c,1,CONSOLE_OUTPUT);   
    
    Write("\n",1,CONSOLE_OUTPUT);
    
    Exit(0);
}