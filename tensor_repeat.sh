#!/bin/bash

for i in $(seq 100); do
    echo $i
    ./tensor > /dev/null
    mv __record__.log __record__100_2.log
    ./postprocess.py
done
