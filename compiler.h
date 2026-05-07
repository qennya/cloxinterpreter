#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

void markCompilerRoots();
ObjFunction* compile(const char* source);

#endif