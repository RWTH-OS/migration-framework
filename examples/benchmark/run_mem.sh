#!/bin/bash

mem_values="262144 524288 1048576 2097152 4194304 8388608 16777216"

for mem in $mem_values; do
	./run.sh -A pandora3 -B pandora4 -V centos660 -n 20 -m $(( mem / 1024 )) >> mem_bench.log
done
