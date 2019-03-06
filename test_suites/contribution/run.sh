args=$(head -n 1 args)
rm -f results/product.csv
rm -f results/sheriff.csv
for ((current=1;current<=10;current++)); do
for fname in product sheriff
do
  rm -f tmp.txt
  END=$ITERATION
  for ((i=1;i<=END;i++)); do
    start=`date +%s%N | cut -b1-13`
    taskset -c 0,2,4,6 execs/$current/$fname.out $args
    #numactl -C 0 -m 0 ./$fname.out $args
    end=`date +%s%N | cut -b1-13`
    runtime=$((end-start))
    echo $runtime >> tmp.txt
  done
  runtime=$(awk '{s+=$1}END{print s/NR}' tmp.txt)
  stddevtime=$(bash stddev.sh tmp.txt)
  rm -f tmp.txt
  echo "$fname, $runtime, $stddevtime" >> results/$fname.csv
done
done
