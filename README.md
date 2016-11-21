<!---
This file is part of migration-framework.
Copyright (C) 2015 RWTH Aachen University - ACS

This file is licensed under the GNU Lesser General Public License Version 3
Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
-->

# migration-framework
[![Build Status](https://travis-ci.org/fast-project/migration-framework.svg?branch=master)](https://travis-ci.org/fast-project/migration-framework)

Allows for initiating start, stop, and migration of virtual machines by means of MQTT messages.

### Requirements
* For migration-framework:
  * libvirt
* For fast-lib:
  * mosquitto
  * mosquittopp
  * yaml-cpp

### Build instructions
```bash
mkdir build && cd build
cmake ..
make
```

### Examples

* See directory "examples" for messages which can be parsed.

* To test messages:  
  1. Start mosquitto daemon (and optional redirect output).
     * "-d" puts mosquitto in background after starting  
     ```bash
     mosquitto -d 2> /dev/null
     ```
  2. Start migfra daemon:  
     
     ```bash
     build/migfra
     ```
  3. Use mosquitto\_pub to publish messages.
     * "-f \<file\>" the file that contains the message content.
     * "-q \<2,1,0\>" sets quality of service. For more information see mosquitto documentation.
     * "-t \<topic\>" the topic to publish on.  
     ```bash
     mosquitto_pub -f examples/start_task.yaml -q 2 -t topic1  
     ```  
  4. Use mosquitto\_sub to see messages.  
     * "-q \<2,1,0\>" sets quality of service. For more information see mosquitto documentation.  
     * "-t \<topic\>" the topic to publish on.
     ```bash
     mosquitto_sub -q 2 -t topic1  
     ```  

* To verify new examples this online yaml parser is useful:  
  http://yaml-online-parser.appspot.com
