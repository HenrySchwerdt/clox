#include <stdio.h>
#include "value.h"
#include "debug.h"

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int simpleInstruction(const char * name, int offset) {
    printf("%-16s\n", name);
   
    return offset + 1;
}

static int constantInstruction(const char* name, int type, Chunk* chunk, int offset) {
    int constant = 0;
    if (type == OP_CONSTANT_LONG) {
        for(int i = 1; i <= 4; i++) {
            constant |= chunk->code[offset+i] << ((4 - i) * 8);
        }
    } else {
        constant = chunk->code[offset+1];
    }
    printf("%s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return type == OP_CONSTANT_LONG ? offset + 5 : offset + 2;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && getLine(&chunk->lines, offset) == getLine(&chunk->lines, offset-1)) {
        printf("   | ");
    } else {
        printf("%4d ", getLine(&chunk->lines, offset));
    }
    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
        case OP_CONSTANT_LONG:
            return constantInstruction("OP_CONSTANT_LONG", OP_CONSTANT_LONG, chunk, offset);
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", OP_CONSTANT, chunk, offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

