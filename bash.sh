set -x
for (( c=100; c<2100; c+=100 ))
do
  # mkdir size$c
  # cd size$c
  # cp ../postprocess.py ../tensor ../tensor.bc ../LLVMInstLoadStore.so ../second_pass/secondpass.so ../runtime/libruntime.so .
  export MSIZE=$c
  # make tensor
  rm record.log
  ./tensor
  python3 postprocess.py record.log
  opt -load ./LLVMInstLoadStore.so -instloadstore -locfile record_output.log < tensor.bc > tensor_inst.bc 2> build_tensor.log
  llc -filetype=obj tensor_inst.bc -o tensor.o
  g++ tensor.o -Wl,./secondpass.so -lpthread -o tensor_2
  ocperf.py stat -e instructions,mem_load_uops_l3_hit_retired.xsnp_hitm -r 100 ./tensor_2 > /dev/null 2> $c.txt
  ocperf.py stat -e instructions,mem_load_uops_l3_hit_retired.xsnp_hitm -r 100 ./tensor_original > /dev/null 2>> $c.txt
done
