INST := 1

all: tensor

false_bc: false.c
	clang -c -emit-llvm false.c -o false.bc
ifdef INST
	opt -load LLVMInstrumenter.so -instrumenter < false.bc > false_inst.bc 2> build_false.log
	mv false_inst.bc false.bc
endif

false: false_inst
	llc -filetype=obj false_inst.bc -o false.o
ifdef INST
	gcc false.o -Wl,./libruntime.so -lpthread -o false
else
	gcc false.o -lpthread -o false
endif

tensor_bc: tensor_parallel.cpp
	clang++ -c -emit-llvm -std=c++11 -I ../eigen/ -DEIGEN_USE_THREADS -DMSIZE=500 tensor_parallel.cpp -o tensor.bc
ifdef INST
	opt -load LLVMInstrumenter.so -instrumenter < tensor.bc > tensor_inst.bc 2> build_tensor.log
	mv tensor_inst.bc tensor.bc
endif

tensor: tensor_bc
	llc -filetype=obj tensor.bc -o tensor.o
ifdef INST
	g++ tensor.o -Wl,./libruntime.so -lpthread -o tensor
else
	g++ tensor.o -lpthread -o tensor
endif

clean:
	rm -f *.bc *.o false tensor
