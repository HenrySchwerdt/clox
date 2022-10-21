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
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_NOT,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
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
bool writeConstant(Chunk* chunk, Value value, int line);
int addConstant(Chunk* chunk, Value value);


#endif