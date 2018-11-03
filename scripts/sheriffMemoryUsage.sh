for (( c=1; c<1001; c+=1 ))
do
  ps aux | grep 'a.out' >> log.out
  sleep 0.005
done
cat log.out | grep '500MB.txt' > sheriffStringMatch.log
awk -v max=0 '{if($5>max){max=$5}}END{print max} ' sheriffStringMatch.log
rm log.out
