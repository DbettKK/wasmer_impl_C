#include <stdio.h>
#include <stdlib.h>

#include "helper.h"

wasm_malloc glob_malloc;
wasm_realloc glob_realloc;
wasm_free glob_free;
void* malloc_cl;
void* free_cl;
void* glob_ctx;
char *linear_memory;

void get_linear_memory(char *mem) {
    linear_memory = mem;
}

void* transfer_i32_to_ptr(int i32) {
    return linear_memory + i32;
}

int transfer_ptr_to_i32(void *ptr) {
    char *cast_ptr = ptr;
    return (cast_ptr - linear_memory) / sizeof(char);
}

void register_ctx(void* ctx) {
    glob_ctx = ctx;
}

void register_malloc(wasm_malloc func, void* mc) {
    glob_malloc = func;
    malloc_cl = mc;
}

void register_realloc(wasm_realloc func) {
    glob_realloc = func;
}
void register_free(wasm_free func, void* fc) { 
    glob_free = func;
    free_cl = fc;
}

void* my_malloc(size_t size) {
    return glob_malloc(size, malloc_cl);
}

void* my_realloc(void* ptr, size_t size) {
    return glob_realloc(ptr, size, glob_ctx);
}

void my_free(void* ptr) {
    return glob_free(ptr, free_cl);
}

