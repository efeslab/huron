#!/bin/bash
"$@" &
pid=$!
while true; do
    line=$(ps auxh -q $pid)
    if [ "$line" == "" ]; then
        break;
    fi
    echo $line >> log.out
    sleep 0.005
done
awk 'BEGIN { maxvsz=0; maxrss=0; } \
    { if ($5>maxvsz) {maxvsz=$5}; if ($6>maxrss) {maxrss=$6}; }\
    END { print "vsz=" maxvsz " rss=" maxrss; }' log.out
rm log.out
