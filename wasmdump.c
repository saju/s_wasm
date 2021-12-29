#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "s_wasm.h"

int read_vector(FILE *fp, vector_t **out_vec);

int read_many_bytes(FILE *fp, size_t size, unsigned char *buffer) {
  size_t read_b;

  read_b = fread(buffer, 1L, (size_t)size, fp);
  if (read_b < size) {
      perror("not enough bytes can be read");
      return -1;
  }
  return read_b;
}

int read_one_byte(FILE *fp) {
  int c;

  c = fgetc(fp);
  if (c == EOF) {
    perror("not enough bytes can be read");
    return EOF;
  }
  return c;
}

int read_u32(FILE *fp, u32 *out){
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
    if ((byte = read_one_byte(fp)) == EOF) {
      return 0;
    }
    _t |= (byte & 0x7F) << (n * 7);
    if (!(byte & 0x80)) /* no continuation high-order bit. we are done */
      break;
  }

  if ((n == 5) && (byte & 0x80)) {
    /* bad LEB encoding. u32 should not take more than 5 bytes, but our encoding sequence
     * has not terminated. 
     */
    fprintf(stderr, "bad encoding of u32\n");
    return 0;
  }
  *out = _t;
  return 1;
}

int read_magic(FILE *fp, module_t *m) {
  unsigned char magic[4];
  
  if (read_many_bytes(fp, 4, magic) != 4) {
    fprintf(stderr, "could not read magic header");
    return 0;
  }
  if ((magic[0] == 0x00) && (magic[1] == 0x61) && (magic[2] == 0x73) && (magic[3] == 0x6d)) {
    m->magic_offset = 0x0;
    return 1;
  } else {
    fprintf(stderr, "bad magic header %#x,%#x,%#x,%#x\n", magic[0], magic[1], magic[2], magic[3]);
    return 0;
  }
}


int read_version(FILE *fp, module_t *m) {
  unsigned char version[4];

  if (read_many_bytes(fp, 4, version) != 4) {
    fprintf(stderr, "could not read version");
    return 0;
  }
  if ((version[0] == 0x01) && (version[1] == 0x00) && (version[2] == 0x00) && (version[3] == 0x00)) {
    m->version.num = version[0];
    m->version.offset = ftell(fp) - 4;
    return 1;
  } else {
    fprintf(stderr, "bad version %#x,%#x,%#x,%#x\n", version[0], version[1], version[2], version[3]);
    return 0;
  }
}

int read_functype(FILE *fp, functype_t **out_func) {
  functype_t *f;
  vector_t *parameters, *results;

  f = malloc(sizeof(functype_t));
  f->parameters = NULL;
  f->results = NULL;

  if (!read_vector(fp, &parameters))
    return 0;

  f->parameters = parameters;

  if (!read_vector(fp, &results))
    return 0;

  f->results = results;
  
  *out_func = f;
  return 1;
}


int __read_v_element(FILE *fp, unsigned char elem_type, vector_element_t **out_element) {
  vector_element_t *e;
  functype_t *func;

  e = malloc(sizeof(vector_element_t));
  e->start_offset = ftell(fp) - 1;
  e->type = elem_type;

  switch(e->type) {
  case 0x60:
    if (!read_functype(fp, &func)) /* XXX: free e */
      return 0;
    e->func = func;
    break;
  case 0x7f:
    /* i32 types do not have internal structure */
    break;
  default:
    printf("NYI - v_element type %#x", e->type);
    return 0;
  }

  *out_element = e;
  return 1;
}
    
  

int read_vector_element(FILE *fp, vector_element_t **out_element) {
  int elem_type;
  
  elem_type = read_one_byte(fp);
  if (elem_type == EOF) {
    fprintf(stderr, "could not read vector element type");
    return 0;
  }

  return __read_v_element(fp, elem_type, out_element);
}
  

int read_vector(FILE *fp, vector_t **out_vec) {
  /*
   * vector = n:u32[element count],n<elements encoding>
   */
  u32 elem_count, tmp;
  vector_t *vec;
  vector_element_t *p;

  vec = malloc(sizeof(vector_t));
  memset(vec, 0, sizeof(vector_t));
  
  vec->start_offset = ftell(fp);

  if (!read_u32(fp, &elem_count)) {
    free(vec);
    return 0;
  }

  vec->num_elems = elem_count;

  for (tmp = 0, p = vec->elements; tmp < elem_count; tmp++) {
    vector_element_t *elem;
    
    if (!read_vector_element(fp, &elem))
      /* XXX: free vec **/
      return 0;

    if (!vec->elements) {
      vec->elements = elem;
      p = vec->elements;
    } else {
      p->next = elem;
      p = p->next;
    }
  }

  *out_vec = vec;
  return 1;
  
}

int read_type_section(FILE *fp, section_t **out_section) {
  /*
   * type section = u32 section length in bytes, vec(function_types)
   */
  u32 length;
  section_t *type_s;
  vector_t *functypes;

  type_s = malloc(sizeof(section_t));
  memset(type_s, 0, sizeof(section_t));

  type_s->type = 0x1;
  type_s->start_offset = ftell(fp) - 1; 

  if (!read_u32(fp, &length)) {
    free(type_s);
    return 0;
  }

  type_s->section_length = length;

  if (!read_vector(fp, &functypes)) {
    return 0;
  }

  type_s->vector = functypes;
  *out_section = type_s;
  
  return 1;
}
    

int read_section(FILE *fp, module_t *m) {
  /*
   * sections = 1 byte section type, u32 section length in bytes, section contents
   */
  int type;
  section_t *section, *tmp;

  type = read_one_byte(fp);
  
  switch(type) {
  case EOF:
    fprintf(stderr, "could not read section type");
    return 0;
  case 0x1:
    if (!read_type_section(fp, &section))
      return 0;
    break;
  default:
    printf("NYI - section(%#x)\n", type);
    break;
  }

  if (!m->sections)
    m->sections = section;
  else {
    for (tmp = m->sections; tmp->next; tmp = tmp->next);
    tmp->next = section;
  }
  return 1;
}

int more_sections(FILE *fp) {
  return 0;
}

int main(int argc, char **argv) {
  module_t m;
  FILE *fp;

  fp = fopen(argv[1], "r");

  if (!fp) {
    perror("failed to open wasm file");
    return 1;
  }

  /* wasm module structure 
   * 4 bytes of magic
   * 4 bytes of version
   * a list of sections 
   */
  memset(&m, 0, sizeof(module_t));
  
  if (!read_magic(fp, &m))
    return 1;

  if (!read_version(fp, &m))
    return 1;

  do {
    if (!read_section(fp, &m)) {
      break;
    }
  } while (more_sections(fp));

  pretty_print_module(&m);

  /* XXX: free malloc'd sections */
}
