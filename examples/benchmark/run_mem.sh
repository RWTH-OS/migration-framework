#!/bin/bash

# This file is part of migration-framework.
# Copyright (C) 2015 RWTH Aachen University - ACS
#
# This file is licensed under the GNU Lesser General Public License Version 3
# Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
#

mem_values="262144 524288 1048576 2097152 4194304 8388608 16777216"

for mem in $mem_values; do
	echo "Running benchmark using $mem MiB RAM"
	./run.sh -A pandora1 -B pandora2 -V centos7113 -n 20 -m $(( mem / 1024 )) | tee -a mem_bench.log
done
