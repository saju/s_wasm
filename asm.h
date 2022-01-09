/* deals with the "asm" instructions */

#include "wasm_types.h"

#define NUMERIC 1

#define NUMERIC_CONST 1

typedef struct {
  byte opcode;
  /* nyi */
} instr_control_t;

typedef struct {
  byte opcode;
  /* nyi */
} instr_ref_t;

typedef struct {
  byte opcode;
  /* nyi */
} instr_var_t;

typedef struct {
  byte opcode;
  /* nyi */
} instr_mem_t;

typedef struct {
  byte opcode;
  byte subtype;
  union { /* we are wasting a ton of space here: XXX */
    i32 i32_const;
    i64 i64_const;
    f32 f32_const;
    f64 f64_const;
  };
} instr_num_t;

typedef struct {
  byte opcode;
  /* nyi */
} instr_vec_t;

struct _instr {
  byte type;
  union {
  instr_control_t ctrl;
  instr_ref_t ref;
  instr_mem_t mem;
  instr_num_t num;
  instr_vec_t vec;
  };
};

instr_t table[] = {
  {NUMERIC, .num = {0x42, NUMERIC_CONST}}, /* i32.const */
  {NUMERIC, .num = {0x43, NUMERIC_CONST}}, /* i64.const */
  {NUMERIC, .num = {0x44, NUMERIC_CONST}}, /* f32.const */
  {NUMERIC, .num = {0x45, NUMERIC_CONST}}, /* f64.const */
  /* NYI */
};

