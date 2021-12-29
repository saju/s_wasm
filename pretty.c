#include <string.h>
#include <stdio.h>
#include "s_wasm.h"

const char *get_type_str(unsigned char type) {
  switch (type) {
  case 0x7f:
    return "i32";
  default:
    return "NYI";
  }
}

void print_types_section(section_t *sec, int indent) {
  int i;
  vector_element_t *p, *m;
  vector_t *params, *results;
  
  printf("[%#09lx]%*sTotal number of functions: %d\n", sec->vector->start_offset, indent, "",
         sec->vector->num_elems);
  for (i = 0, p = sec->vector->elements; i < sec->vector->num_elems; i++, p = p->next) {
    printf("[%#09lx]%*sFunction(%d) has %d parameters and %d return values\n",  p->start_offset,
           indent+4, "", i, p->func->parameters->num_elems, p->func->results->num_elems);
    
    params = p->func->parameters;
    results = p->func->results;
    
    printf("[%#09lx]%*sParameter types: ", params->start_offset, indent+8, "");


    for (m = params->elements; m; m = m->next) 
      printf("%s ", get_type_str(m->type));

    printf("\n");
    
    printf("[%#09lx]%*sResults types: ", results->start_offset, indent+8, "");

    for (m = results->elements; m; m = m->next) 
      printf("%s ", get_type_str(m->type));

    printf("\n");
  }
}


void pretty_print_module(module_t *m) {
  int indent = 0;
  section_t *sec;

  printf("[%#09lx] magic header (\\0asm)\n", m->magic_offset);
  printf("[%#09lx] version number (%#x)\n", m->version.offset, m->version.num);

  for (sec = m->sections; sec; sec = sec->next) {
    printf("[%#09lx] section begins, type %#x, size %#lx bytes\n", sec->start_offset, sec->type,
           sec->section_length);
    if (sec->type == 0x1) {
      print_types_section(sec, indent+4);
    } else {
      printf("NYI - Section");
    }
  }
}
