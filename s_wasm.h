#ifndef __S_WASM_H__
#define __S_WASM_H__

#include <stdlib.h>

#define S_WASM_INDEX 0x88

typedef uint32_t u32;

typedef struct _vector_element vector_element_t;
typedef struct _vector vector_t;
typedef struct _section section_t;

enum vectype {typed, indices};

typedef struct {
  unsigned char foo; /* XXX: NYI */
} resulttype_t;

typedef struct {
  vector_t *parameters;
  vector_t *results;
} functype_t;

struct _vector_element {
  size_t start_offset;
  unsigned char type;
  union {
    functype_t *func;
    u32 index;
    /* other types are NYI */
  };
  vector_element_t *next;
};

struct _vector {
  size_t start_offset;
  u32 num_elems;
  vector_element_t *elements; /* vector elements are a linked list */
};


struct _section {
  unsigned char type;
  size_t start_offset;
  size_t section_length;
  vector_t *vector;
  section_t *next;
};
  
typedef struct {
  size_t magic_offset;
  struct {
    unsigned char num;
    size_t offset;
  } version;
  section_t *sections; /* a singly linked list of sections */
} module_t;


void pretty_print_module(module_t *);

#endif /* __S_WASM_H__ */
