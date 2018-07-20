#!/bin/bash

#time out-static/db_bench --threads=4 --benchmarks=fillbatch,readseq
#time out-static/db_bench --threads=4 --db=/mnt/ramdisk/test.db --benchmarks=fillbatch,readseq

#time out-static/db_fs_bench-buggy --threads=4 --benchmarks=fillbatch,readseq
#time out-static/db_fs_bench-fixed --threads=4 --benchmarks=fillbatch,readseq

DB_NAME=/mnt/ramdisk/leveldb/tmi-benchmark.leveldb
DB_SIZE=--num=10000000 --value_size=2 --db=$(DB_NAME) 
SEQ_READ_ARGS=--threads=4 --benchmarks=readseq $(DB_SIZE) --use_existing_db=1

CRC_ARGS=--threads=4 --benchmarks=crc32c

NUM_TRIALS=15

#FILTER_OUTPUT=2> /dev/null | tail -n1
FILTER_OUTPUT=

init:
	read -p "delete $(DB_NAME)? " IGNORE_RESULT
	rm -rf $(DB_NAME)
	out-static/db_fs_bench-fixed $(DB_SIZE) --benchmarks=fillbatch

tmi:
	sudo ~/timen.py --iters $(NUM_TRIALS) --log ~/leveldb-seqread.tmi.times ~/tmi/build/bin/tmildr out-static/db_fs_bench-buggy $(SEQ_READ_ARGS) $(FILTER_OUTPUT)

buggy:
	~/timen.py --iters $(NUM_TRIALS) --log ~/leveldb-seqread.buggy.times out-static/db_fs_bench-buggy $(SEQ_READ_ARGS) $(FILTER_OUTPUT)

fixed:
	~/timen.py --iters $(NUM_TRIALS) --log ~/leveldb-seqread.fixed.times out-static/db_fs_bench-fixed $(SEQ_READ_ARGS) $(FILTER_OUTPUT)
