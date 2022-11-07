#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *linear_memory;

void get_linear_memory(char *mem);

void* transfer_i32_to_ptr(int i32);

int transfer_ptr_to_i32(void *ptr);

void* call_func(int func_offset, int argc, void **argv);

extern void* glob_ctx;

typedef void* (*wasm_malloc)(size_t size, void* ctx);
typedef void* (*wasm_realloc)(void* ptr, size_t size, void* ctx);
typedef void (*wasm_free)(void* ptr, void* ctx);

extern wasm_malloc glob_malloc;
extern wasm_realloc glob_realloc;
extern wasm_free glob_free;

extern void* malloc_cl;
extern void* free_cl;

void register_ctx(void* ctx);
void register_malloc(wasm_malloc func, void* mc);
void register_realloc(wasm_realloc func);
void register_free(wasm_free func, void* fc);
void* my_malloc(size_t size);
void* my_realloc(void* ptr, size_t size);
void my_free(void* ptr);

#define malloc(size) my_malloc(size)
#define realloc(ptr, size) my_realloc(ptr, size)
#define free(ptr) my_free(ptr);

#endif // HELPER_H