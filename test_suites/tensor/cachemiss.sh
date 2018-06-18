set -x
for (( c=100; c<1600; c+=100 ))
do
  # mkdir size$c
  # cd size$c
  # cp ../postprocess.py ../tensor ../tensor.bc ../LLVMInstLoadStore.so ../second_pass/secondpass.so ../runtime/libruntime.so .
  export MSIZE=$c
  ocperf.py stat -e cache-misses,mem_load_uops_l3_hit_retired.xsnp_hitm -r 100 ./tensor_benchmark > /dev/null 2>> padded.txt
  done
