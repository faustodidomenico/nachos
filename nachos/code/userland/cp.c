// CP utilitary program. 
#include "syscall.h"

int main(int argc, char** argv){

    if(argc != 3){
        Write("Arguments error\n", 17, CONSOLE_OUTPUT);
        Exit(-1);
    }

    OpenFileId src = Open(argv[1]);
    
    if(src < 2){
        Write("Failed opening file. \n", 24, CONSOLE_OUTPUT);
        Exit(-1);
    }

    Create(argv[2]);
    
    OpenFileId dst = Open(argv[2]);

    char c[1];
    
    while(Read(c, 1, src, -1) > 0) Write(c,1,dst);   
    
    Exit(0);
}