source head.sh
for fname in args instrumented.out original.out product.out sheriff.out
do
  exit_if_file_not_found $fname
done
args=$(head -n 1 args)
rm -f time.csv
for fname in original product sheriff
do
  rm -f tmp.txt
  END=$ITERATION
  for ((i=1;i<=END;i++)); do
    start=`date +%s%N | cut -b1-13`
    ./$fname.out $args
    end=`date +%s%N | cut -b1-13`
    runtime=$((end-start))
    echo $runtime >> tmp.txt
  done
  runtime=$(awk '{s+=$1}END{print s/NR}' tmp.txt)
  stddevtime=$(bash stddev.sh tmp.txt)
  rm -f tmp.txt
  echo "$fname, $runtime, $stddevtime" >> time.csv
done

