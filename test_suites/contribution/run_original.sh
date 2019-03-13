source head.sh
for fname in args original.out
do
  exit_if_file_not_found $fname
done
args=$(head -n 1 args)
rm -f results/original.csv
for fname in original
do
  rm -f tmp.txt
  END=$ITERATION
  for ((i=1;i<=END;i++)); do
    start=`date +%s%N | cut -b1-13`
    taskset -c 0,2,4,6 ./$fname.out $args
    #numactl -C 0 -m 0 ./$fname.out $args
    end=`date +%s%N | cut -b1-13`
    runtime=$((end-start))
    echo $runtime >> tmp.txt
  done
  runtime=$(awk '{s+=$1}END{print s/NR}' tmp.txt)
  stddevtime=$(bash stddev.sh tmp.txt)
  rm -f tmp.txt
  echo "$fname, $runtime, $stddevtime" >> results/original.csv
done

