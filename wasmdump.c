#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "s_wasm.h"

#define VEC_SET_STORAGE(v, ptr, type) {                         \
 ptr = v->nelts * sizeof(type) <= VEC_DEFAULT_SIZE        \
     ? v->__storage                                           \
     : calloc(v->nelts, sizeof(type));                          \
 }

void bye(char *msg, ...) {
  va_list p;

  va_start(p, msg);
  vfprintf(stderr, msg, p);
  va_end(p);
  exit(1);
}

int read_many_bytes(FILE *fp, size_t size, unsigned char *buffer) {
  size_t read_b;

  read_b = fread(buffer, 1L, (size_t)size, fp);
  if (read_b < size) {
    bye("not enough bytes could be read\n");
    return -1;
  }
  return read_b;
}

byte read_one_byte(FILE *fp) {
  int c;

  c = fgetc(fp);
  if (c == EOF) {
    bye("hit EOF while reading 1 more byte\n");
  }
  return c;
}

u32 read_u32(FILE *fp){
  /* 
   * u32 is LEB128 encoded variable length unsigned integer with 32 bits of information. 
   * This will fit nicely into uint32_t storage on most platforms.
   *
   * Webassembly spec section 5.2.2 puts the additional constraint that a uN integer should not 
   * be encoded in more than ceil(N/7) bytes. So we know that a u32 cannot take up more than 5 bytes
   * after encoding.
   */
  u32 _t;
  int n, byte;

  for (n = 0, _t = 0; n < 5; n++) {
    byte = read_one_byte(fp);

    _t |= (byte & 0x7F) << (n * 7);
    if (!(byte & 0x80)) /* no continuation high-order bit. we are done */
      break;
  }

  if ((n == 5) && (byte & 0x80)) {
    /* bad LEB encoding. u32 should not take more than 5 bytes, but our encoding sequence
     * has not terminated. 
     */
    bye("bad encoding of u32\n");
  }
  return _t;
}

int read_magic(FILE *fp, module_t *m) {
  unsigned char magic[4];
  
  read_many_bytes(fp, 4, magic);
  if ((magic[0] == 0x00) && (magic[1] == 0x61) && (magic[2] == 0x73) && (magic[3] == 0x6d)) {
    m->magic = 1;
    return 1;
  } else {
    bye("bad magic header %#x,%#x,%#x,%#x\n", magic[0], magic[1], magic[2], magic[3]);
    return 0;
  }
}


int read_version(FILE *fp, module_t *m) {
  unsigned char version[4];

  read_many_bytes(fp, 4, version);
  if ((version[0] == 0x01) && (version[1] == 0x00) && (version[2] == 0x00) && (version[3] == 0x00)) {
    m->version = 1;
    return 1;
  } else {
    bye("bad version %#x,%#x,%#x,%#x\n", version[0], version[1], version[2], version[3]);
    return 0;
  }
}

vector_t *read_vec_valtype1(FILE *fp) {
  vector_t *v;
  u32 i;

  v = calloc(1, sizeof(vector_t));
  v->type = 0x88;
  v->nelts = read_u32(fp);

  v->pvalues = ((v->nelts * sizeof(valtype_t)) <= VEC_DEFAULT_SIZE)
    ? v->__storage
    : calloc(v->nelts, sizeof(valtype_t));

  for (i = 0; i < v->nelts; i++) {
    byte val_type = read_one_byte(fp);
    if (val_type == 0x7f) {
      v->pvalues[i].type = 0x7f;
      v->pvalues[i].u_32 = read_u32(fp);
    } else {
      printf("NYI --- valtype %x\n", val_type);
    }
  }
  return v;
}

vector_t *read_vec_valtype(FILE *fp) {
  /*
   * Sec 5.3.1, 5.3.3 & 5.3.4
   *
   * valtype ::= 𝑡:numtype ⇒ 𝑡
   *           | 𝑡:reftype ⇒ 𝑡
   *
   * numtype ::= 0x7F ⇒ i32 
   *           | 0x7E ⇒ i64 
   *           | 0x7D ⇒ f32 
   *           | 0x7C ⇒ f64
   *
   * reftype ::= 0x70 ⇒ funcref
   *           | 0x6F ⇒ externref
   */
  vector_t *v;
  u32 i;

  v = calloc(1, sizeof(vector_t));
  v->type = 0x88;
  v->nelts = read_u32(fp);
  
  VEC_SET_STORAGE(v, v->pvaltypes, byte);

  for (i = 0; i < v->nelts; i++) {
    v->pvaltypes[i] = read_one_byte(fp);
  }
  return v;
}

functype_t *read_functype(FILE *fp) {
  /*
   * Sec 5.3.6 & 5.3.5
   * 
   * functype ::= 0x60 rt1:resulttype rt2:resulttype ⇒ rt1 → rt2
   * 
   * resulttype ::= 𝑡*: vec(valtype) ⇒ [𝑡*]
   */
  functype_t *f;
  byte type;

  type = read_one_byte(fp);
  if (type != 0x60) {
    bye("functype: was expecting to read type(0x60), but got type(%#x)\n", type);
  }
  f = calloc(1, sizeof(functype_t));
  
  f->parameters = read_vec_valtype(fp);
  f->results = read_vec_valtype(fp);

  return f;
}

vector_t *read_vec_functype(FILE *fp) {
  vector_t *v;
  u32 i;

  v = calloc(1, sizeof(vector_t));
    
  v->nelts = read_u32(fp);
  v->type = 0x60;

  VEC_SET_STORAGE(v, v->pfuncs, functype_t);

  for (i = 0; i < v->nelts; i++) {
    v->pfuncs[i] = read_functype(fp);
  }
  return v;
}

void read_section(FILE *fp, module_t *m) {
  /*
   * section𝑁(B) ::= 𝑁:byte size:u32 cont:B ⇒ cont (if size = ||B||) 
   *               |  𝜖                      ⇒  𝜖
   */
  section_t *s;

  s = calloc(1, sizeof(section_t));

  s->offset = ftell(fp);
  s->type = read_one_byte(fp);
  s->len = read_u32(fp);
  
  if (s->type == 0x1) {
    /*
     * typesec ::= ft * : section1 (vec(functype)) ⇒ ft *
     */
    s->v = read_vec_functype(fp); 
    m->typesec = s;
  }
}


int more_sections(FILE *fp) {
  /* is there more to read from the wasm file */
  int c;
  return 0;
  c = fgetc(fp);
  if (c == EOF)
    return 0;
  else {
    ungetc(c, fp);
    return 1;
  }
}

int main(int argc, char **argv) {
  module_t m;
  FILE *fp;


  fp = fopen(argv[1], "r");

  if (!fp) {
    bye("could not open wasm file: %s\n", argv[1]);
    return 1;
  }

  /* wasm module structure 
   * 4 bytes of magic
   * 4 bytes of version
   * a list of sections 
   */
  memset(&m, 0, sizeof(module_t));
  
  read_magic(fp, &m);
  read_version(fp, &m);

  do {
    read_section(fp, &m);
  } while (more_sections(fp));

  pretty_print_module(&m);

  /* XXX: free malloc'd sections */
}
