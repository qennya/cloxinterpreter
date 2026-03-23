#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main() {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    int constant = addConstant(&chunk, 1);
    for (int i = 0; i < 300; i++) {
        writeChunk(&chunk, OP_CONSTANT, 123);
        writeChunk(&chunk, constant, 123);
    }

    for (int i = 0; i < 299; i++) {
        writeChunk(&chunk, OP_ADD, 123);
    }

    writeChunk(&chunk, OP_RETURN, 123);

    interpret(&chunk);

    freeChunk(&chunk);
    freeVM();

    return 0;
}