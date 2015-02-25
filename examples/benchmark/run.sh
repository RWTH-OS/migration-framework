#!/bin/bash

n=1

while getopts A:B:h opt; do
	case $opt in
	A)
		server_a="$OPTARG"
		;;
	B)
		server_b="$OPTARG"
		;;
	n)
		n="$OPTARG"
		;;
	h)
		echo -e "Usage: `basename $0` -A serverA -B serverB

		-A serverA
		Hostname of first server to start migfra on.

		-B serverB
		Hostname of second server to start migfra on." | sed 's/^\s\s*//g'
		exit 0
		;;
	esac
done


if [ -z "$server_a" ] && [ -z "$server_b" ]; then
	echo "No scheduler and server hosts passed as arguments."
elif [ -n "$server_a" ] && [ -n "$server_b" ]; then
	# start migfra on servers using ssh in background
	echo "Start migfra on servers."
	ssh -f "$server_a" "`realpath $0` -A $server_a"
	ssh -f "$server_b" "`realpath $0` -B $server_b"

	# start migration benchmark
	`dirname $0`/../../build/migfra_benchmark -n $n -V "vm1" -t "." -H pandora3 -A pandora1 -B pandora2 -m 1024

	# quit migfra on servers
	echo "Stop migfra on servers"
	mosquitto_pub -t "topic-a" -q 2 -f "`dirname $0`/quit_task.yaml"
	mosquitto_pub -t "topic-b" -q 2 -f "`dirname $0`/quit_task.yaml"

elif [ -n "$server_a" ] || [ -n "$server_b" ]; then
	if [ -n "$server_a" ]; then
		server="$server_a"
		config="server_a.conf"
	else
		server="$server_b"
		config="server_b.conf"
	fi
	echo "Starting migfra on $server"
	echo "Using conf: $config"
	cd `dirname $0`
	if [ -f "../../build/migfra" ]; then
		../../build/migfra --config "$config" & disown
	else
		echo "Cannot find migfra executable on $server."
		exit 1
	fi
else
	echo "Please specify hosts for server A and server B."
	echo "-h to show usage."
	exit 1
fi
