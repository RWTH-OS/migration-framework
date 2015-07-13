#!/bin/sh

# Establish environment for parastation mpi
. "/opt/parastation_gcc_4.4.7/bin/mpivars.sh"

# Add pscom (git version) to library path
export LD_LIBRARY_PATH="$HOME/pscom/lib:$LD_LIBRARY_PATH"

# Test if parastaion daemon is running
if [ ! "$(pidof psid)" ]; then
	echo "psid is not running"
	echo "please run \"/etc/init.d/parastation start\" as root"
fi
