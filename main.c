#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    int constant = addConstant(&chunk, 1.2);
    // (3 + 4) * -2 = -14

    int c1 = addConstant(&chunk, 3);
    int c2 = addConstant(&chunk, 4);
    int c3 = addConstant(&chunk, 2);

    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, c1, 123);

    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, c2, 123);

    writeChunk(&chunk, OP_ADD, 123);

    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, c3, 123);

    writeChunk(&chunk, OP_NEGATE, 123);

    writeChunk(&chunk, OP_MULTIPLY, 123);

    writeChunk(&chunk, OP_RETURN, 123);


    disassembleChunk(&chunk, "test chunk");

    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return 0;
}