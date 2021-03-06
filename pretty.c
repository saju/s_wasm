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

void print_funcsec(module_t *m, int indent) {
  vector_t *v = m->funcsec->v;
  int i;

  printf("[%09lx]%*s function section (%#lx bytes)\n", m->funcsec->offset, indent, "", m->funcsec->len);

  for (i = 0; i < v->nelts; i++) {
    printf("%*s index (%d) from code sec is mapped to type definition index %d\n",
           indent+4, "", i, v->pindices[i]);
  }
}

void print_exportssec(module_t *m, int indent) {
  vector_t *v = m->exportssec->v;
  int i;
  
  printf("[%09lx]%*s exports section (%#lx bytes)\n", m->exportssec->offset, indent, "",
         m->exportssec->len);

  for (i = 0; i < v->nelts; i++) {
    export_t *exp = v->pexports[i];

    if (exp->desc == 0x0) {
      printf("%*s function (idx=%x), %s\n", indent+4, "", exp->idx, exp->name);
    } else {
      printf("%*s export type(%#x) NYI (idx=%x), %s\n", indent+4, "", exp->desc, exp->idx, exp->name);
    }
  }
}

void print_codesec(module_t *m, int indent) {
  vector_t *v = m->codesec->v;
  int i;

  printf("[%09lx]%*s code section (%#lx bytes)\n", m->codesec->offset, indent, "", m->codesec->len);

  for (i = 0; i < v->nelts; i++) {
    code_t *code = v->pcodes[i];
    printf("%*s func %d locals: i32(%d), i64(%d), f32(%d), f64(%d), funcref(%d), "
           "externref(%d), vector(%d)\n", indent+4, "", i, code->num_i32_locals, code->num_i64_locals,
           code->num_f32_locals, code->num_f64_locals, code->num_funcref_locals,
           code->num_externref_locals, code->num_vec_locals);
  }
}
    
    
    
    

void pretty_print_module(module_t *m) {
  int indent = 0;
  
  printf("[%09lx]%*s magic (\\0asm)\n", 0x0L, indent, "");
  printf("[%09lx]%*s version (0x1)\n", 0x4L, indent, "");

  if (m->typesec)
    print_typesec(m, indent);
  if (m->funcsec)
    print_funcsec(m, indent);
  if (m->exportssec)
    print_exportssec(m, indent);
  if (m->codesec)
    print_codesec(m, indent);
}
  
