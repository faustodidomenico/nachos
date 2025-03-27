#include "lib/list.hh"

class Path{
public:
    Path(const char* path, bool isFile = false);
    ~Path();
    List<char*> GetPath();
    const char* GetFileName();
    char* AppendToRaw(const char* path);
    bool IsPathToFile();
    bool IsRelative();
    unsigned Length();

private:
    List<char*> *dirPath;
    const char* fileName;
    bool isRelative;
    unsigned pathLength;
    const char* rawPath;
};


// Dado: /home/federico/test.txt

// Creamos una clase donde tenemos todos los nombres 'parseados'
// Con referencias al directorio padre. 