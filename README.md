<!---
This file is part of migration-framework.
Copyright (C) 2015 RWTH Aachen University - ACS

This file is licensed under the GNU Lesser General Public License Version 3
Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
-->

# migration-framework
Allows for initiating start, stop, and migration of virtual machines by means of MQTT messages.

#####################
### Installation: ###
#####################

Requirements: 
libmosquittopp, yaml-cpp & libvirt installed.

mkdir build && cd build
cmake ..
make
make install #(not implemented yet)

#################
### Examples: ###
#################

See examples/ for messages which can be parsed.
To test messages:
1. Start mosquitto daemon.
	mosquitto &
2. Start migfra daemon:
	build/migfra
3. Use mosquitto_pub to publish messages, eg.:
	mosquitto_pub -f examples/start_task.yaml -q 2 -t topic1
4, Use mosquitto_sub to see messages, eg.:
	mosquitto_sub -q 2 -t topic1
5. Look into log:
	journalctl -n 20

To verify new examples this online yaml parser is useful:
http://yaml-online-parser.appspot.com

#######################
### Logging system: ###
#######################

You can choose the logging system to use in CMakeLists.txt.
See comments in there for explanation.
Systemd is recommended.
