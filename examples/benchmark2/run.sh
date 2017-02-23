#!/bin/bash

# This file is part of migration-framework.
# Copyright (C) 2017 RWTH Aachen University - ACS
#
# This file is licensed under the GNU Lesser General Public License Version 3
# Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
#

help_msg="Usage `basename $0` -r <remote> -A <domain> -B <domain> [-n <number of rounds] [-h]

-r <remote>
Set remote server to migrate to/from localhost

-A <domain>
The first domain to migrate initially from localhost.

-B <domain>
The second domain to migrate initially from remote host.

-n <number of rounds> (=10)
Repeat <number of rounds> times.
Defaults to 10 rounds.

-h
This help message."

n=10
host=$(hostname)
remote=""
domain_a=""
domain_b=""
dir=$(dirname $0)

# Handle argument options.
while getopts r:n:A:B:h opt; do
	case $opt in
	r)
		remote="$OPTARG"
		;;
	n)
		n="$OPTARG"
		;;
	A)
		domain_a="$OPTARG"
		;;
	B)
		domain_b="$OPTARG"
		;;
	h)
		echo -e "$help_msg"
		exit 0
		;;
	esac
done

# Check if remote is set.
if [ -z "$remote" ]; then
	echo "Please pass the remote servers hostname using -r"
	echo -e "$help_msg"
	exit 0
fi

# Check if domain_a is set.
if [ -z "$domain_a" ]; then
	echo "Please pass the domains name running locally using -A"
	echo -e "$help_msg"
	exit 0
fi

# Check if domain_b is set.
if [ -z "$domain_b" ]; then
	echo "Please pass the domains name running remotely using -B"
	echo -e "$help_msg"
	exit 0
fi

# Check if migfra is running.
if ! pidof migfra > /dev/null; then
	echo "Please start the migration-framework."
	exit 0
fi

function check_state {
	local domain="$1"
	local server="$2"
	local desired_state="$3"
	local state=$(virsh -c qemu+ssh://$server/system domstate $domain)
	if [ "$state" != "$desired_state" ]; then
		echo "$domain is not $desired_state (state: $state)"
		exit
	fi
}

function migrate_migfra {
	local domain="$1"
	local host="$2"
	local dest="$3"
	local rdma="false"
	local msg=$(cat $dir/migrate_task.yaml | sed "s/vm-name-placeholder/$domain/g" | sed "s/destination-placeholder/$dest/g" | sed "s/rdma-migration-placeholder/$rdma/g")
	echo "$msg"
	mosquitto_sub -t "fast/migfra/$host/result" -h zerberus -q 2 -C 1 &
	mosquitto_pub -t "fast/migfra/$host/task" -h zerberus -q 2 -m "$msg"
	wait $!
	if [ $? -ne 0 ]; then
		echo "Error in mosquitto_sub"
	fi
}

function migrate_virsh {
	local domain="$1"
	local host="$2"
	local dest="$3"
	local rdma="false"
	miguri="$dest"
	if [ "$rdma" == "true" ]; then
		miguri="--migrateuri rdma://$dest-ib"
	fi
	echo $miguri
	virsh -c "qemu+tcp://$host/system" migrate --domain "$domain" --desturi "qemu+ssh://$dest/system" #"$miguri"
}

function swap_migfra {
	local domain_a="$1"
	local domain_b="$2"
	local host="$3"
	local dest="$4"
	local rdma="false"
	local msg=$(cat $dir/migrate_swap_task.yaml | sed "s/vm-name-a-placeholder/$domain_a/g" | sed "s/vm-name-b-placeholder/$domain_b/g" | sed "s/destination-placeholder/$dest/g" | sed "s/rdma-migration-placeholder/$rdma/g")
	echo "$msg"
	mosquitto_sub -t "fast/migfra/$host/result" -h zerberus -q 2 -C 1 &
	mosquitto_pub -t "fast/migfra/$host/task" -h zerberus -q 2 -m "$msg"
	wait $!
	if [ $? -ne 0 ]; then
		echo "Error in mosquitto_sub"
	fi
}

function swap_virsh {
	echo bla
}

#check_state centos7114 pandora2 running
#migrate_migfra centos7114 pandora2 pandora1
#check_state centos7114 pandora1 running
#migrate_virsh centos7114 pandora1 pandora2

function migrate_func {
	local task=$1
	for ((i=0;i<n;++i)); do
		check_state "$domain_a" "$host" running
		time $task "$domain_a" "$host" "$remote"
		check_state "$domain_a" "$remote" running
		time $task "$domain_a" "$remote" "$host"
	done |& tee "$dir/output.log"
	echo $task
	gawk -F 's|m|\\s' 'BEGIN{cnt=0;sum=0} $1 == "real"{print $3;++cnt;sum+=$3} END{print sum/cnt}' "$dir/output.log"
}
function swap_func {
	local task=$1
	for ((i=0;i<n;++i)); do
		check_state "$domain_a" "$host" running
		check_state "$domain_b" "$remote" running
		time $task "$domain_a" "$domain_b" "$host" "$remote"
		check_state "$domain_a" "$remote" running
		check_state "$domain_b" "$host" running
		time $task "$domain_a" "$domain_b" "$remote" "$host"
	done |& tee "$dir/output.log"
	echo $task
	gawk -F 's|m|\\s' 'BEGIN{cnt=0;sum=0} $1 == "real"{print $3;++cnt;sum+=$3} END{print sum/cnt}' "$dir/output.log"
}
migrate_func migrate_migfra
migrate_func migrate_virsh
swap_func swap_migfra
