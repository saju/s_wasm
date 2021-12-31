#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "s_wasm.h"

const char *get_type_str(byte type) {
  switch (type) {
  case 0x7f:
    return "i32";
  default:
    return "NYI";
  }
}

void print_resulttypes(vector_t *v) {
  int i;

  for (i = 0; i < v->nelts; i++) {
    printf("%s ", get_type_str(v->pvaltypes[i]));
  }
}

void print_typesec(module_t *m, int indent) {
  vector_t *v = m->typesec->v;
  int i;
  
  printf("[%09lx]%*s type section (%#lx bytes)\n", m->typesec->offset, indent, "", m->typesec->len);

  for (i = 0; i < v->nelts; i++) {
    vector_t *vf;
    
    printf("%*sFunction[%d]\n", indent+4, "", i);
    
    vf = v->pfuncs[i]->parameters;
    printf("%*sparameters(%"PRIu32"): ", indent+8, "", vf->nelts);
    print_resulttypes(vf); printf("\n");
    
    vf = v->pfuncs[i]->results;
    printf("%*sresults(%"PRIu32"): ", indent+8, "", vf->nelts);
    print_resulttypes(vf); printf("\n");
  }
}

void pretty_print_module(module_t *m) {
  int indent = 0;
  
  printf("[%09lx]%*s magic (\\0asm)\n", 0x0L, indent, "");
  printf("[%09lx]%*s version (0x1)\n", 0x4L, indent, "");

  print_typesec(m, indent);
}
  
