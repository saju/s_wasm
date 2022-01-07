#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "s_wasm.h"

#define VEC_SET_STORAGE(v, ptr, type) {                   \
 ptr = v->nelts * sizeof(type) <= VEC_DEFAULT_SIZE        \
     ? v->__storage                                       \
     : calloc(v->nelts, sizeof(type));                    \
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

export_t *read_export(FILE *fp) {
  /*
   * export     ::= nm:name 𝑑:exportdesc       ⇒ {name nm , desc 𝑑} 
   * exportdesc ::= 0x00 𝑥:funcidx             ⇒ func 𝑥
   *             |  0x01 𝑥:tableidx            ⇒ table 𝑥
   *             |  0x02 𝑥:memidx              ⇒ mem 𝑥
   *             |  0x03 𝑥:globalidx           ⇒ global 𝑥
   */
  export_t *e;
  u32 size;

  e = calloc(1, sizeof(export_t));

  size = read_u32(fp);

  e->name = malloc(size + 1);
  e->name[size] = '\0';
  read_many_bytes(fp, size, e->name);
  
  e->desc = read_one_byte(fp);
  e->idx = read_u32(fp);

  return e;
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

vector_t *read_vec_indices(FILE *fp) {
  vector_t *v;
  u32 i;

  v = calloc(1, sizeof(vector_t));

  v->nelts = read_u32(fp);
  v->type = 0x89;

  VEC_SET_STORAGE(v, v->pindices, u32);

  for (i = 0; i < v->nelts; i++) {
    v->pindices[i] = read_u32(fp);
  }

  return v;
}


vector_t *read_vec_exports(FILE *fp) {
  vector_t *v;
  u32 i;

  v = calloc(1, sizeof(vector_t));
  v->nelts = read_u32(fp);
  v->type = 0x7;

  VEC_SET_STORAGE(v, v->pexports, export_t);

  for (i = 0; i < v->nelts; i++) {
    v->pexports[i] = read_export(fp);
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
  } else if (s->type == 0x3) {
    /*
     * funcsec ::= 𝑥* :section3 (vec(typeidx)) ⇒ 𝑥*
     *
     * NB: this section ties the type section together with the code section.
     * The index of this section matches the index of the code section, while
     * the value of that corresponding index matches the index of the type section.
     */
    s->v = read_vec_indices(fp);
    m->funcsec = s;
  } else if (s->type == 0x7) {
    /*
     * exportsec  ::= ex*:section7 (vec(export)) ⇒ ex*
     */
    s->v = read_vec_exports(fp);
    m->exportssec = s;
  }
  else {
    /*
     *
     * NYI section
     */
    s->v = NULL;
    printf("Section type(%#x), size(%#lx bytes) is NYI ... skipping\n", s->type, s->len);
    fseek(fp, s->len, SEEK_CUR);
  }
}


int main(int argc, char **argv) {
  module_t m;
  FILE *fp;
  size_t file_size;


  fp = fopen(argv[1], "r");

  if (!fp) {
    bye("could not open wasm file: %s\n", argv[1]);
    return 1;
  }

  fseek(fp, 0L, SEEK_END);
  file_size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);
  
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
  } while (ftell(fp) < file_size);

  pretty_print_module(&m);

  /* XXX: free malloc'd sections */
}
