clang++ -c -emit-llvm -std=c++11 -I ../eigen/ -DEIGEN_USE_THREADS tensor_parallel.cpp -o tensor.bc
opt -load ./LLVMInstLoadStore.so -instloadstore < tensor.bc > tensor_inst.bc 2> build_tensor.log
mv tensor_inst.bc tensor.bc
llc -filetype=obj tensor.bc -o tensor.o
g++ tensor.o -Wl,./secondpass.so -lpthread -o tensor
