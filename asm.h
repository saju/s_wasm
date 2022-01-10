/* deals with the "asm" instructions */

#include "wasm_types.h"


#define NUMERIC 1

#define I32_CONST 1
#define I64_CONST 2
#define F32_CONST 3
#define F64_CONST 4


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
  byte opcode;
  byte type;
  const char *desc;
  union {
    instr_control_t ctrl;
    instr_ref_t ref;
    instr_mem_t mem;
    instr_num_t num;
    instr_vec_t vec;
  };
};

#include "opcodes.h"

