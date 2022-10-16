#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "utils.h"
#include "value.h"
#include "line_tracker.h"

#define CONSTANT_LONG_BYTE_SIZE 4



typedef enum {
    OP_CONSTANT_LONG,
    OP_CONSTANT,
    OP_NEGATE,
    OP_RETURN,
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    LineArray lines;
    ValueArray constants;
} Chunk;


void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void writeConstant(Chunk* chunk, Value value, int line);
int addConstant(Chunk* chunk, Value value);


#endif