for (( dir=7; dir<=11; dir++ ))
do 
  if [[ $# -ge 1 ]];
  then
    cd $dir
    rm -f -r summary
    mkdir -p summary
    cp ../mallocRuntimeIDs.txt .
    for filename in ./77*.txt; do
      /home/takh/Documents/huron/postprocess/postprocess detect $filename test.txt > /dev/null
    done
    cd ..
    cat $dir/summary/*.txt > $dir.txt
    rm -r $dir/summary/
  fi
  echo -n $dir,
  cat $dir.txt | cut -f1 -d , |sort | uniq | wc -l
done
