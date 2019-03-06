set -x
clang -g -c -emit-llvm toy.c -o main.bc
opt -load /home/takh/Documents/huron/build/llvm-passes/Instrumenter/LLVMInstrumenter.so -instrumenter < main.bc > main.inst.bc 2> inst.log
llc -filetype=obj main.inst.bc -o instrumented.o
clang instrumented.o -Wl,/home/takh/Documents/huron/runtime/libruntime.so -pthread -o instrumented.out

