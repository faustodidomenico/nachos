#include "path.hh"
#include <string.h>
#include <stdlib.h>
// relativo = nachos/code
// abs = /home/federico/dev/so2/nachos/code/filesys/test/big
using namespace std;

/* Construye un path a partir de un string. Dependiendo de si es un archivo, guarda el nombre del mismo en la propiedad fileName.
    Se guarda el nombre de cada subdirectorio que conforma el path en una lista.
    Ademas detecta si el path es relativo o absoluto. */

Path::Path(const char* path, bool isFile){
    rawPath = path;
    isRelative  = path[0] != '/';
    dirPath = new List<char*>();
    fileName = nullptr;
    pathLength = 0;
    if (!isRelative) {
        char root[2];
        strcpy(root,"/");
        dirPath->Append(root);
        pathLength++;
    }

    char *path_ = new char[20];
    strcpy(path_,path);
    
    char *temp, *token = strtok( (char*) path_, "/");

    while(token != nullptr) {
        temp = token;
        token = strtok(nullptr, "/");
        if(isFile && token == nullptr)
            fileName = temp;
        else {
            dirPath->Append(temp);
            pathLength++;
        }
    }
}

Path::~Path(){
    delete dirPath;
}

List<char*>
Path::GetPath(){
    return *dirPath;
}

const char*
Path::GetFileName(){
    return fileName;
}

bool
Path::IsPathToFile(){
    return fileName != nullptr;
}

bool
Path::IsRelative(){
    return isRelative;
}

unsigned
Path::Length(){
    return pathLength;
}

/* Agrega al path crudo con el que se construyo esta instancia y lo retorna. 
    Utilizado para hacer comparaciones. */
char* 
Path::AppendToRaw(const char* path){
    char *result = (char *) malloc(sizeof(char) * 60);   // array to hold the result.

    strcpy(result,rawPath); // copy string one into the result.
    strcat(result,"/");
    strcat(result,path);

    return result;
}


