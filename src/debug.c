#include <stdio.h>
#include "../include/value.h"
#include "../include/debug.h"

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== start %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
	printf("== end %s  ==\n", name);

}

static int simpleInstruction(const char * name, int offset) {
    printf("%-16s\n", name);
   
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name , slot);
    return offset + 2;
}

static int constantInstruction(const char* name, int type, Chunk* chunk, int offset) {
    int constant = 0;
    if (type == OP_CONSTANT_LONG || 
        type == OP_DEFINE_GLOBAL_LONG || 
        type == OP_GET_GLOBAL_LONG || 
        type == OP_SET_GLOBAL_LONG) {
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
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", OP_DEFINE_GLOBAL, chunk, offset);
        case OP_DEFINE_GLOBAL_LONG:
            return constantInstruction("OP_DEFINE_GLOBAL_LONG", OP_DEFINE_GLOBAL_LONG, chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", OP_GET_GLOBAL, chunk, offset);
        case OP_GET_GLOBAL_LONG:
            return constantInstruction("OP_GET_GLOBAL_LONG", OP_GET_GLOBAL_LONG, chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", OP_SET_GLOBAL, chunk, offset);
        case OP_SET_GLOBAL_LONG:
            return constantInstruction("OP_SET_GLOBAL_LONG", OP_SET_GLOBAL_LONG, chunk, offset);
		case OP_NIL:
			return simpleInstruction("OP_NIL", offset);
		case OP_TRUE:
			return simpleInstruction("OP_TRUE", offset);
		case OP_FALSE:
			return simpleInstruction("OP_FALSE", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
		case OP_EQUAL:
			return simpleInstruction("OP_EQUAL", offset);
		case OP_GREATER:
			return simpleInstruction("OP_GREATER", offset);
		case OP_LESS:
			return simpleInstruction("OP_LESS", offset);
		case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
		case OP_NOT:
			return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

