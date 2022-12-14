#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/common.h"
#include "../include/compiler.h"
#include "../include/scanner.h"
#include "../include/utils.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);


typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool final;
} Local;

typedef struct {
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;


Parser parser;

Compiler* current = NULL;

Chunk* compilingChunk;

static Chunk* currentChunk() {
    return compilingChunk;
}

// TODO needs refactoring
static void errorAt(Token* token, const char* message) {

    if (parser.panicMode) return;

    int lineNr = token->charPosition < parser.previous.charPosition ? token->line-1: token->line;
    int length = 0;
    char* line = getSourceLine(&length, lineNr);
    int charPos = token->charPosition == 1 ||token->charPosition < parser.previous.charPosition ? 4 + length : token->charPosition;
    parser.panicMode = true;
    fprintf(stderr, "%s[line %d:%d] Error", ANSI_COLOR_RED, lineNr, charPos);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n%s", message, ANSI_COLOR_RESET);
    // print line before 
    if (lineNr > 1) {
        int beforeLength = 0;
        char * beforeLine = getSourceLine(&beforeLength, lineNr - 1);
        fprintf(stderr,"%s\t%-4d|%s %.*s\n%s",CYN, lineNr -1, ANSI_COLOR_RESET, beforeLength, beforeLine, CYN);
    }
    fprintf(stderr,"\t%-4d|%s %.*s\n %s",lineNr, ANSI_COLOR_RESET, length, line, MAG);
    fprintf(stderr, "\t");
    
    for (int i = 0;i <= charPos; i++) {
        if (i < 5) {
            fprintf(stderr, " ");
        } else {
            fprintf(stderr, "-");
        }
        
    }
    fprintf(stderr,"^\n%s", ANSI_COLOR_RESET);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;
    for(;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }

}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static int emitConstant(Value value) {
    int index = writeConstant(currentChunk(), value, parser.previous.line);
    if(index == -1){
        error("Too many constants in one chunk");
    }
    return index;
}

static void patchJump(int offset) {
    int jump = currentChunk()->count - offset - 2;
    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if(!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;
    while(current->localCount > 0 &&
            current->locals[current->localCount -1].depth >
                current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    } 
}


static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static uint32_t identifierConstant(Token* name) {
    int constant = addConstant(currentChunk(), OBJ_VAL(copyString(name->start, name->length)));
    return (u_int32_t) constant;
}

static bool identifierEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount -1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifierEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static void addLocal(Token name, bool isFinal) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->final = isFinal;
    local->depth = current->scopeDepth;
}

static void declareVariable(bool isFinal) {

    if (current->scopeDepth == 0) return;
    Token* name = &parser.previous;
    for (int i = current->localCount -1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }
        if (identifierEqual(name, &local->name)) {
            error("Already variable with this name in this scope");
        }
    }
    addLocal(*name, isFinal);
}

static uint32_t parseVariable(const char* errorMessage) {
    bool isFinal = parser.previous.type == TOKEN_VAL;
    consume(TOKEN_IDENTIFIER, errorMessage);
    declareVariable(isFinal);
    if (current->scopeDepth > 0) return 0;
    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint32_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    if (global <= UINT8_MAX) {
        emitBytes(OP_DEFINE_GLOBAL, (uint8_t) global);
    } else if (global <= UINT32_MAX) {
        uint8_t largeConstant[CONSTANT_LONG_BYTE_SIZE];
        CONVERT_TO_BYTE_ARRAY(largeConstant, CONSTANT_LONG_BYTE_SIZE, global);
        emitByte(OP_DEFINE_GLOBAL_LONG);
        for (int i = 0; i < CONSTANT_LONG_BYTE_SIZE; i++) {
            emitByte(largeConstant[i]);
        }
    } else {
        error("Too many constants in one chunk");
    }
}

static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void string(bool canAssign) {
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
						parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = arg <= UINT8_MAX ? OP_GET_LOCAL : OP_GET_LOCAL_LONG;
        setOp = arg <= UINT8_MAX ? OP_SET_LOCAL : OP_SET_LOCAL_LONG;
    } else {
        arg = identifierConstant(&name);
        getOp = arg <= UINT8_MAX ? OP_GET_GLOBAL : OP_GET_GLOBAL_LONG;
        setOp = arg <= UINT8_MAX ? OP_SET_GLOBAL : OP_SET_GLOBAL_LONG;
    }

    if (getOp == OP_GET_LOCAL_LONG || getOp == OP_GET_GLOBAL_LONG) {
        uint8_t largeConstant[CONSTANT_LONG_BYTE_SIZE];
        CONVERT_TO_BYTE_ARRAY(largeConstant, CONSTANT_LONG_BYTE_SIZE, arg);
        if (match(TOKEN_EQUAL) && canAssign) {
            Local* local = &current->locals[arg];
            if (local->final) {
                error("Can't reassign final variable");
            }
            expression();
            emitByte(setOp);
        } else {
            emitByte(getOp);
        }
        // Adds 4 bytes of memory to the chunk
        for (int i = 0; i < CONSTANT_LONG_BYTE_SIZE; i++) {
                emitByte(largeConstant[i]);
        }
    } else {
        if (match(TOKEN_EQUAL) && canAssign) {
            Local* local = &current->locals[arg];
            if (local->final) {
                error("Can't reassign final variable");
            }
            expression();
            emitBytes(setOp, arg);
        } else {
             emitBytes(getOp, (uint8_t) arg);
        }
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    // compile the operand
    parsePrecedence(PREC_UNARY);
    // Emit the operator instruction
    switch (operatorType)
    {
		case TOKEN_BANG: emitByte(OP_NOT); break;
		case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default:
            return; // Unreachable
    }

}



static void binary(bool canAssign) {
    // Remember the operator
    TokenType operatorType = parser.previous.type;
    // previous: +, current: 1

    // Compile the rig`
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    // Emit the operator instruction
    switch (operatorType)
    {
		case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
		case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
		case TOKEN_GREATER: emitByte(OP_GREATER); break;
		case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
		case TOKEN_LESS: emitByte(OP_LESS); break;
		case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS: emitByte(OP_ADD); break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
        default:
            return; // Unreachable
    }
}

static void literal(bool canAssign) {
	switch(parser.previous.type) {
		case TOKEN_FALSE: emitByte(OP_FALSE); break;
		case TOKEN_NIL: emitByte(OP_NIL); break;
		case TOKEN_TRUE: emitByte(OP_TRUE); break;
		default:
			break; // Unreachable
	}

}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL,PREC_NONE},
    [TOKEN_RIGHT_PAREN]= {NULL,NULL,PREC_NONE},
    [TOKEN_LEFT_BRACE]= {NULL,NULL,PREC_NONE},
    [TOKEN_RIGHT_BRACE]= {NULL,NULL,PREC_NONE},
    [TOKEN_COMMA]= {NULL,NULL,PREC_NONE},
    [TOKEN_DOT]= {NULL,NULL,PREC_NONE},
    [TOKEN_MINUS]= {unary,binary, PREC_TERM},
    [TOKEN_PLUS]= {NULL,binary, PREC_TERM},
    [TOKEN_SEMICOLON]= {NULL,NULL, PREC_NONE},
    [TOKEN_SLASH]= {NULL,binary, PREC_FACTOR},
    [TOKEN_STAR]= {NULL,binary, PREC_FACTOR},
    [TOKEN_BANG]= {unary,NULL,PREC_NONE},
    [TOKEN_BANG_EQUAL]= {NULL,binary,PREC_EQUALITY},
    [TOKEN_EQUAL]= {NULL,NULL,PREC_NONE},
    [TOKEN_EQUAL_EQUAL]= {NULL,binary,PREC_EQUALITY},
    [TOKEN_GREATER]= {NULL,binary,PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,binary,PREC_COMPARISON},
    [TOKEN_LESS]= {NULL,binary,PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]= {NULL,binary,PREC_COMPARISON},
    [TOKEN_IDENTIFIER]= {variable,NULL,PREC_NONE},
    [TOKEN_STRING]= {string,NULL,PREC_NONE},
    [TOKEN_NUMBER]= {number,NULL,PREC_NONE},
    [TOKEN_AND]= {NULL,and_,PREC_AND},
    [TOKEN_CLASS]= {NULL,NULL,PREC_NONE},
    [TOKEN_ELSE]= {NULL,NULL,PREC_NONE},
    [TOKEN_FALSE]= {literal,NULL,PREC_NONE},
    [TOKEN_FOR]= {NULL,NULL,PREC_NONE},
    [TOKEN_FUN]= {NULL,NULL,PREC_NONE},
    [TOKEN_IF]= {NULL,NULL,PREC_NONE},
    [TOKEN_NIL]= {literal,NULL,PREC_NONE},
    [TOKEN_OR]= {NULL,or_,PREC_OR},
    [TOKEN_PRINT]= {NULL,NULL,PREC_NONE},
    [TOKEN_RETURN]= {NULL,NULL,PREC_NONE},
    [TOKEN_SUPER]= {NULL,NULL,PREC_NONE},
    [TOKEN_THIS]= {NULL,NULL,PREC_NONE},
    [TOKEN_TRUE]= {literal,NULL,PREC_NONE},
    [TOKEN_VAR]= {NULL,NULL,PREC_NONE},
    [TOKEN_VAL]= {NULL,NULL,PREC_NONE},
    [TOKEN_WHILE]= {NULL,NULL,PREC_NONE},
    [TOKEN_ERROR]= {NULL,NULL,PREC_NONE},
    [TOKEN_EOF]= {NULL,NULL,PREC_NONE},
};



static void parsePrecedence(Precedence precedence) {
    advance(); 
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);
    while(precedence <= getRule(parser.current.type)->precedence) {
        advance(); 
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }
    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}



static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration() {
    uint32_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(global);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void whileStatement() {
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    statement();

    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);
    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}

static void synchronize() {
    parser.panicMode = false;
    while(parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;

        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_VAL:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                // do nothing
                ;
        }
        advance();
      
    }
}

static void declaration() {
    if (match(TOKEN_VAR) || match(TOKEN_VAL)) {
        varDeclaration();
    } else {
        statement();
    }
    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

bool compile(const char* source, Chunk* chunk) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;
    advance();
    while(!match(TOKEN_EOF)) {
        declaration();
    }
    endCompiler();
    return !parser.hadError;
}
