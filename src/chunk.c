#include <stdlib.h>
#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    initLineArray(&chunk->lines);
    initValueArray(&chunk->constants);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    // if chunk full free memory
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    writeLineArray(&chunk->lines, line);

    chunk->count++;
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    freeLineArray(&chunk->lines);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void writeConstant(Chunk* chunk, Value value, int line) {
    int index = addConstant(chunk, value);
    if (index > UINT8_MAX) {
        u_int8_t largeConstant[4];
        for (int i = 0; i < 4; i++) {
            largeConstant[i] = index >> ((3-i) * 8);
        }
        writeChunk(chunk, OP_CONSTANT_LONG, line);
        for (int i = 0; i < 4; i++) {
            writeChunk(chunk, largeConstant[i], line);
        }
    } else {
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, index, line);
    }
}


