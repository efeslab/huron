set -x
clang -c -emit-llvm lu.c -D_LINUX_ -o a.bc
opt -load ~/eigen-optim/LLVMInstrumenter.so -instrumenter < a.bc > a.inst.bc 2> a.log
llc -filetype=obj a.inst.bc -o a.o
gcc a.o -Wl,/home/takh/eigen-optim/libruntime.so -lpthread -lm -o a.out
