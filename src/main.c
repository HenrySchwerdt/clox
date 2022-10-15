#include <stdio.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"


int main(int argc, const char * argv[]) {
    Chunk chunk;
    initChunk(&chunk);
    for (int i = 0; i< 260; i++) {
        writeConstant(&chunk, 0.0 + i, i);
    }
    
    writeChunk(&chunk, OP_RETURN, 124);
    writeChunk(&chunk, OP_RETURN, 124);
    writeChunk(&chunk, OP_RETURN, 125);
    writeChunk(&chunk, OP_RETURN, 125);
    
    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);
    return 0;
}