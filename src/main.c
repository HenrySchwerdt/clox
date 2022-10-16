#include <stdio.h>
#include "common.h"
#include "chunk.h"
#include "vm.h"
#include "debug.h"


int main(int argc, const char * argv[]) {
    initVM();
    Chunk chunk;
    initChunk(&chunk);
    writeConstant(&chunk, 1.2, 1);
    writeChunk(&chunk, OP_NEGATE, 2);
    writeChunk(&chunk, OP_RETURN, 3);
    
    disassembleChunk(&chunk, "test chunk");
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return 0;
}