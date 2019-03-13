set -x
clang -g -c -emit-llvm toy.c -o main.bc
#opt -load /home/takh/Documents/huron/build/llvm-passes/MallocDependent/LLVMMallocDependent.so -mallocdependent < main.bc > /tmp/main.bc 2> mallocdependent.log
opt -load /home/takh/Documents/huron/build/llvm-passes/RedirectPtr/LLVMRedirectPtr.so -redirectptr -locfile address_translation_table.txt -depfile Andersen.txt < main.bc > main.redirected.bc 2> redirect.log
llc -filetype=obj main.redirected.bc -o redirected.o
clang redirected.o -pthread -o product.out
clang redirected.o -rdynamic ~/Documents/huron/others/sheriff/sheriff/libsheriff_protect64.so -ldl -o sheriff.out
