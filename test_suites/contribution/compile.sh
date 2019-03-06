source head.sh
exit_if_file_not_found profile_args
args=$(head -n 1 profile_args)
echo $args
bash clean.sh
bash compile_profile_pass.sh
./instrumented.out $args
~/Documents/huron/postprocess/postprocess all record.log address_translation_table.txt
bash compile_product_pass.sh
clang -g toy.c -pthread -o original.out
#clang -g toy.c -c -o sheriff.o
#clang sheriff.o -rdynamic ~/Documents/huron/others/sheriff/sheriff/libsheriff_protect64.so -ldl -o sheriff.out
