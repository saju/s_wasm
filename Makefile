CC ?= clang
CFLAGS ?= -g -Wall -I.

SRCS := $(wildcard *.c)

opcodes.h:
	python3 opcode_gen.py

wasmdump: opcodes.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

gen_wasm:
	cd test && wat2wasm test.wat
	cd test && wat2wasm constants.wat

all: gen_wasm wasmdump

clean:
	rm -f *.o *~ a.out wasmdump opcodes.h
	rm -rf *.dSYM
	rm -f test/*.wasm
