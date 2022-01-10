#include <stdio.h>
#include "asm.h"
#include "s_wasm.h"

instr_t *read_instructions(FILE *fp) {
  while (read_one_byte(fp) != 0x0b) {};
  return NULL;
}
