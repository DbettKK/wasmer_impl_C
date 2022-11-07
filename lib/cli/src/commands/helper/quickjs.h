#ifndef QUICKJS_H
#define QUICKJS_H

#include <stdio.h>
#include <inttypes.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

typedef struct JSContext JSContext;
typedef struct JSRuntime JSRuntime;
typedef int BOOL;


typedef struct JSMallocState {
    size_t malloc_count;
    size_t malloc_size;
    size_t malloc_limit;
    int opaque; /* user opaque */
} JSMallocState;

typedef struct JSMallocFunctions {
    int js_malloc;
    int js_free;
    int js_realloc;
    int js_malloc_usable_size;
} JSMallocFunctions;

typedef enum {
  JS_GC_OBJ_TYPE_JS_OBJECT,
  JS_GC_OBJ_TYPE_FUNCTION_BYTECODE,
  JS_GC_OBJ_TYPE_SHAPE,
  JS_GC_OBJ_TYPE_VAR_REF,
  JS_GC_OBJ_TYPE_ASYNC_FUNCTION,
  JS_GC_OBJ_TYPE_JS_CONTEXT,
} JSGCObjectTypeEnum;

typedef struct  {
  int ref_count; /* must come first, 32-bit */
  JSGCObjectTypeEnum gc_obj_type : 4;
  uint8_t mark : 4; /* used by the GC */
  uint8_t dummy1;   /* not used by the GC */
  uint16_t dummy2;  /* not used by the GC */
  int link;
} JSGCObjectHeader;

struct JSContext {
    JSGCObjectHeader header;
    int rt;
};  

struct JSRuntime {
    JSMallocFunctions mf;
    JSMallocState malloc_state;
};

#endif // QUICKJS_H