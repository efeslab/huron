all: false

false_bc: false.c
	clang -c -emit-llvm false.c -o false.bc
ifndef NOINST
	opt -load LLVMInstrumenter.so -instrumenter < false.bc > false_inst.bc 2> build_false.log
endif

false: false_bc
ifndef NOINST
	llc -filetype=obj false_inst.bc -o false.o
	gcc false.o -lruntime -lpthread -o false
else
	llc -filetype=obj false.bc -o false.o
	gcc false.o -lpthread -o false
endif

clean:
	rm -f *.bc *.o false
