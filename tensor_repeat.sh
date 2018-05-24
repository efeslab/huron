#!/bin/bash

wdir=$1

for i in $(seq $2); do
    echo $i
    ./tensor > /dev/null
    mv __record__.log "$wdir/__record__.log"
    ./postprocess.py "$wdir/__record__.log"
    mv "$wdir/__record__.log" "$wdir/100_$i.log"
    mv "$wdir/__record___output.log" "$wdir/100_${i}_output.log"
done
mv "$wdir/__record___stable.csv" "$wdir/100.csv"
