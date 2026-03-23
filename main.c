#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(void) {
    Chunk chunk;
    initChunk(&chunk);

    for (int i = 0; i < 260; i++) {
        writeConstant(&chunk, NUMBER_VAL(i), 123);
    }

    writeChunk(&chunk, OP_RETURN, 123);

    disassembleChunk(&chunk, "test chunk");

    freeChunk(&chunk);
    return 0;
}