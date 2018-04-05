MQTT Topics
-----------

### Structure
```
fast
+-- migfra
|   +-- <hostname>
|   |   +-- task
|   |   +-- result
|   +-- ...
|   +-- <hostname>
+-- pscom
    +-- <hostname>
    |   +-- <procID>
    |   |   +-- request
    |   |   +-- response
    |   |-- ...
    |   |-- <procID>
    +-- ...
    +-- <hostname>
```

### Subscriber
* fast/migfra/\<hostname\>/task
  Requests from external instance (e.g., the scheduler) such as start/stop/migrate
* fast/pscom/\<hostname\>/\<procID\>/response
  Pscom informs that all non-migratable connections have been closed

### Publisher
* fast/migfra/\<hostname\>/result
  Reports status of tasks (e.g., domain successfully started)
* fast/pscom/\<hostname\>/\<procID\>/request
  Request pscom to shutdown all non-migratable connections

Message Format
--------------
Messages are formated by using the YAML scheme. The different messages are
sorted by their sources.

### Input
#### Start Domains
Request from external instance (e.g., the scheduler) to start one or more guests
on the respective computing node.
* topic: fast/migfra/\<hostname\>/task
* Payload

```
host: <string>
task: start vm
id: <uuid>
vm-configurations:
  - vm-name: <string>
    memory: <unsigned long (in kiB)>
    vcpus: <count>
  - xml: <XML string>
    pci-ids:
      - vendor: <vendor-id>
        device: <device-id>
      - ..
    transient: <bool>
  - ..
```
* id: The ID is included in the result message and may be used for tracking according tasks and results.
* VM may be started either by  "name" or by "xml".
* vm-name: Searches for an already defined VM (virDomainLookupByName).
* xml: A new VM is being defined using the XML string (virDomainDefineXML).
* overlay-image & base-image are defined in the XML.
* memory: Set memory and maxmemory (optional).
* vcpus: Set vcpus and maxvcpus (optional).
* pci-ids: May be used to pass a list of PCI-IDs in order to attach PCI devices with according IDs.
  PCI-IDs describe the device type and may be easily detected for a specific device using "lspci -nn".
  The PCI-ID consists of vendor ID and device ID.
  e.g.:
```
  - vendor: 0x15b3
    device: 0x1004
```
* transient: May be used to start a VM as transient domain. A transient domain is only defined during runtime and becomes unknown to libvirt after being shut down. Here, the domain has to be started using XML. (optional)
* Expected behavior:
  Starts domains on specified host.
  Sends result message after waiting for the domain to properly start (probing with ssh).

#### Stop Domain
* topic: fast/migfra/\<hostname\>/task
* Payload

```
host: <string>
task: stop vm
id: <uuid>
list:
  - vm-name: <vm name>
  - vm-name: <vm name>
    force: <bool>
    undefine: <bool>
  - ..
```
* force: Enables forced shutdown of the domain using virDomainDestroy instead of virDomainShutdown (default:`false`).
* undefine: Undefine the domain after shutdown which works like `virsh undefine <vm name>` (default: `false`).
* Expected behavior:
  Domains are stopped.
* Answer: Default result status.

#### Suspend Domain
Stop to assign the domain(s) to any of the physical CPUs.
* topic: fast/migfra/\<hostname\>/task
* Payload

```
host: <string>
task: suspend vm
id: <uuid>
list:
  - vm-name: <vm name>
  - vm-name: <vm name>
  - ..
```
* Expected bahavior:
  Domain(s) are suspended.
* Response: Default result status

#### Resume Domain
Re-assing physical CPU cores to the respective domain(s).
* topic: fast/migfra/\<hostname\>/task
* Payload

```
host: <string>
task: resume vm
id: <uuid>
list:
  - vm-name: <vm name>
  - vm-name: <vm name>
  - ..
```
* Expected behavior:
  Domain(s) are resumed.
* Response: Default result status

#### Migrate Domain
Requst the migration of a given domain from the respective computing node to
another cluster node specified within the message body.
* topic: fast/migfra/\<hostname\>/task
* Payload

```
host: <string>
task: migrate vm
id: <uuid>
vm-name: <vm name>
destination: <destination hostname>
time-measurement: <bool>
parameter:
  retry-counter: <counter>
  migration-type: <live | warm | offline>
  rdma-migration: <bool>
  pscom-hook-procs: <count of processes>
  vcpu-map: [[<cpus>], [<cpus>], ...]
  swap-with:
    vm-name: <vm name>
    pscom-hook-procs: <count of processes>
    vcpu-map: [[<cpus>], [<cpus>], ...]
```
* time-measurement: Returns the duration of each migration phase in the result message. (Optional)
* pscom-hook-procs: Number of processes of which the pscom layer has to be suspended. (Optional)
* vcpu-map: Enables to reassign VCPUs to CPUs on the destination system. See [CPU Repin](#cpu-repin). (Optional)
* swap-with: Enables to swap two domains. Here, pscom-hook-procs and vcpu-map may be specified for the second domain. The domain which is specified in swap-with has to run on the "destination" host.
* Expected behavior:
  Domain is being migrated to the destination node.
* Answer: Default result status


#### Evacuate node
Request from external instance (e.g., the scheduler) to evacuate the respective
computing node.
* topic: fast/migfra/\<hostname\>/task
* Payload

```
host: <string>
task: evacuate node
id: <uuid>
time-measurement: <bool>
destinations:
  - <destination hostname>
  - ...
parameter:
  retry-counter: <counter>
  mode: <auto | compact | scatter>
  migration-type: <live | warm | offline>
  rdma-migration: <bool>
  overbooking: <bool>
  pscom-hook-procs: <count of processes>
```
* id: Is returned in the response message for the matching of tasks and results.
* destinations: a lists of possible destination nodes
* time-measurement: enable/disable time measurements
* retry-counter: the maximum amount of retries per domain
* mode
  auto: domains-to-destination mapping chosen by migfra
  compact: fill up destination by destination
  scatter: equally distribute the domains to the provided destinations
* migration-type:
  - live: keep domain running (e.g., pre-copy migration)
  - warm: suspend domain before migration
  - offline: use file system for migraiton
* rdma-migration: migrate domains by using the RDMA transport
* overbooking: allow an overbooking of the destination nodes
* pscom-hook-procs: the amount of pscom processes per domain (equal distribution assumed)

#### Repin CPUs
Facilitates a remapping of virtual CPUs to the physical CPUs of the host system.
* topic: fast/migfra/\<hostname\>/task
* Payload

```
task: repin vm
id: <uuid>
vm-name: <string>
vcpu-map: [[<cpus>], [<cpus>], ...]
```
* vcpu-map contains assignment of VCPUs to CPUs.
E.g. Assignment of VCPU 0 to CPUs 4, 1 to 5, 2 to 6, and 3 to 7:
```
vcpu-map: [[4],[5],[6],[7]]
```
E.g. Assignment of VCPUs 0-4 to CPUs 4-7:
```
vcpu-map: [[4,5,6,7],[4,5,6,7],[4,5,6,7],[4,5,6,7]]
```

### Output
#### Domain started
This message is emitted once the domain is started and ready to execute an
application.
* topic: fast/migfra/\<hostname\>/result
* Payload:

```
scheduler: <hostname/global>
result: vm started
id: <uuid>
list:
  - vm-name: <vm-hostname>
    status: <success | error>
    details: <string>
    process-id: <process id of the vm>
  - vm-name: <vm-hostname>
    status: <success | error>
    process-id: <process id of the vm>
  - ..
```
* details: Here, detailed information on the error may be included.
* Expected behavior:
  The scheduler responsible for the node receives the message and starts the application in the domain.
* Implementation:
  Using mqtt_publish in startup script in domain. Here, we have to possible ways:
	1. Domain sends "vm ready" to migfra and migfra sends it concluded to the scheduler as "vm started".
	2. Migfra waits for all domains to be contactable using SSH and sends a summary result to the scheduler.
  Currently 2. is used in implementation.

#### Domain stopped
This message is emitted once the domain is shutdown successfully.
* topic: fast/migfra/\<hostname\>/result
* Payload

```
result: vm stopped
id: <uuid>
list:
  - vm-name: <vm-hostname>
    status: <success | error>
    details: <string>
  - vm-name: <vm-hostname>
    status: <success | error>
  - ..
```

* details: Ermöglicht detailierte Fehlerinformationen zurückzugeben.
* Expected behavior:
  Cleanup domain? Extract log files from domain for user?

#### Domain suspended
This message is emitted once the domain is suspended successfully.
* topic: fast/migfra/\<hostname\>/result
* Payload

```
result: vm suspended
id: <uuid>
list:
  - vm-name: <vm-hostname>
    status: <success | error>
    details: <string>
  - vm-name: <vm-hostname>
    status: <success | error>
  - ..
```

* details: Here, detailed information on the error may be included.

#### Domain resumed
This message is emitted once the domain is resumed successfully.
* topic: fast/migfra/\<hostname\>/result
* Payload

```
result: vm resumed
id: <uuid>
list:
  - vm-name: <vm-hostname>
    status: success | error
    details: <string>
  - vm-name: <vm-hostname>
    status: success | error
  - ..
```

* details: Here, detailed information on the error may be included.



#### Migration finished
This message is emitted once the migration has terminated. This either denotes
the successful migration of the respective domain to the destination or an
error during the migration process.
* topic: fast/migfra/\<hostname\>/result
* Payload

```
result: vm migrated
id: <uuid>
vm-name: <vm name>
status: <success | error>
details: <retries | error-string>
process-id: <process id of the vm>
time-measurement:
  - <tag>: <duration in sec>
  - ..
```
* details: Here, detailed information on the error may be included or the number of retries on success.
* time-measurement: If time-measurement was activated in the task, a map of tags with durations is returned here.
* Expected behavior:
  Scheduler marks original resources as free.

#### Node evacutated
This message is emitted once all domains are move to other cluster nodes.
* topic: fast/migfra/\<hostname\>/result
* Payload

```
result: node evacuated
id: <uuid>
time-measurement:
  - <tag>: <duration in sec>
  - ..
list
  - vm-name: <vm name>
    status: <success | error>
    details: <retries | error-string>
    process-id: <process id of the vm>
    time-measurement:
      - <tag>: <duration in sec>
      - ..
  - ...
```
* list: contains the status of all domains that have been located on the source
        node
* details: provides detailed information in case of failures; in case of
           "success" it contains the amount of retries for that domain
* time-measurement: returns a list of tags with the time measurements per domain
  and for the whole evacuation process
* Expected behavior:
  In case of success, the source node does not have running domains anymore


#### CPU repinning done
This message is emitted once the repinning has been performed.
* topic: fast/migfra/\<hostname\>/result
* Payload

```
result: vm repinned
id: <uuid>
vm-name: <vm name>
status: <success | error>
details: <error-string>
```
* details: Here, detailed information on the error may be included.

#### Shutdown connections
This message requests the pscom layer to execute the S/R protocol for all
non-migratable connections.
* topic: fast/pscom/\<hostname\>/\<pid\>/request
* Payload

```
task: suspend
```

* Expected behavior:
  pscom executes suspend protocol.

#### Reconnect connections
This message requests the pscom-layer to release the previously shut down
connections.
* topic: fast/pscom/\<hostname\>/\<pid\>/request
* Payload

```
task: resume
```

* Expected behavior:
  pscom resumes connections.
