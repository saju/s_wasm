#ifndef __S_WASM_H__
#define __S_WASM_H__

#include <stdlib.h>

#define S_WASM_INDEX 0x88
#define VEC_DEFAULT_SIZE 0xA

typedef uint32_t u32;
typedef unsigned char byte;

typedef struct _vector vector_t;

typedef struct {
  vector_t *parameters;
  vector_t *results;
} functype_t;

typedef struct {
  byte type;
  union {
    u32 u_32;
    /* more types NYI */
  };
} valtype_t;

/*
 * Vectors contain homogenous items. Every vector instance gets VEC_DEFAULT_SIZE bytes of storage, so 
 * we don't have to dynamically allocate storage as we parse vectors. The vector payload will point into
 * __storage unless nelts > VEC_DEFAULT_SIZE, in which case we will malloc() the storage.
 */
struct _vector {
  u32 nelts;
  byte type;
  union {
    functype_t **pfuncs;
    u32 *pindices;
    valtype_t *pvalues;
    byte *pvaltypes;
  };
  void *__storage[VEC_DEFAULT_SIZE];
};

/*
 * section = 1byte (type encoding) : u32 (length in bytes) : vec(<content>)
 */
typedef struct {
  size_t offset;
  size_t len;
  byte type;
  vector_t *v;
} section_t;

typedef struct {
  unsigned int magic:1;
  unsigned int version:1;
  section_t *typesec;
  section_t *funcsec;
} module_t;


void pretty_print_module(module_t *);

#endif /* __S_WASM_H__ */
