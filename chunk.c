#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0; //we start with 0
    chunk->capacity = 0; //no memeory yet
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity; // save old size to allocator
        chunk->capacity = GROW_CAPACITY(oldCapacity); //calc new size
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, //
            oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines,
        oldCapacity, chunk->capacity);

    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void writeConstant(Chunk* chunk, Value value, int line) {
    int constantIndex = addConstant(chunk, value);

    if (constantIndex <= UINT8_MAX) {
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, (uint8_t)constantIndex, line);
    } else if (constantIndex <= 0xFFFFFF) {
        writeChunk(chunk, OP_CONSTANT_LONG, line);
        writeChunk(chunk, (constantIndex >> 16) & 0xFF, line);
        writeChunk(chunk, (constantIndex >> 8) & 0xFF, line);
        writeChunk(chunk, constantIndex & 0xFF, line);
    } else {
        fprintf(stderr, "Too many constants in one chunk.\n");
        exit(1);
    }
}