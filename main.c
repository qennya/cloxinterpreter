#include <stdio.h>

#include "chunk.h"
#include "debug.h"

int main(void) {
    Chunk chunk;
    initChunk(&chunk);

    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_RETURN, 124);
    writeChunk(&chunk, OP_RETURN, 124);
    writeChunk(&chunk, OP_RETURN, 125);

    disassembleChunk(&chunk, "test chunk");

    printf("\nManual line checks:\n");
    for (int i = 0; i < chunk.count; i++) {
        printf("instruction %d -> line %d\n", i, getLine(&chunk, i));
    }

    freeChunk(&chunk);
    return 0;
}