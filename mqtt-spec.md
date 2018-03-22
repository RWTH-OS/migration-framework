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
    vcpus: <Anzahl>
  - xml: <XML string>
    pci-ids:
      - vendor: <vendor-id>
        device: <device-id>
      - ..
    transient: <bool>
  - ..
```
* id: Wird bei result Nachricht mit zurück geschickt, um die Zugehörigkeit zwischen task/result erfassen zu können.
* VM kann entweder per "name" gestartet werden oder per "xml"
* vm-name: Es wird eine schon definierte VM gesucht (virDomainLookupByName)
* xml: Es wird eine VM anhand des XML-Strings definiert (virDomainDefineXML)
* overlay-image & base-image sind im XML definiert.
* memory: Setzt memory und maxmemory. (optional)
* vcpus: Setzt vcpus und maxvcpus. (optional)
* pci-ids: Ermöglicht eine Liste von PCI-IDs anzugeben, um je ein PCI-Device mit dieser ID zu attachen.
  PCI-IDs geben den Geräte-Typ an und können mithilfe von "lspci -nn" leicht herausgefunden werden.
  Sie bestehen aus vendor und device ID.
  Bsp.:
```
  - vendor: 0x15b3
    device: 0x1004
```
* transient: Ermöglicht ein VM mittels XML als transiente Domain zu starten. Eine transiente Domain ist nur während der Laufzeit definiert und ist nach dem Herunterfahren nicht mehr für Libvirt bekannt. Hier ist die Übergabe des XML notwendig. (optional)
* Erwartetes Verhalten:
  VMs werden auf dem entsprechenden Host gestartet.
  Es wird gewartet bis die VMs bereit (erreichbar mit ssh) sind bevor result geschickt wird.
* Antwort: Default result status? Oder doch lieber modifiziert mit eindeutige IDs für die VMs?

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
    force: <true/false>
    undefine: <true/false>
  - ..
```
* force: Ermöglicht es optional die VM unmittelbar zu beenden (virDomainDestroy statt virDomainShutdown; default:`false`)
* undefine: Ermöglicht es optional die VM unmittelbar nach dem Herunterfahren aus der Liste der libvirt bekannten VMs herauszunehmen (implizit entspricht dies dem Aufruf `virsh undefine <vm name>`; default: `false`)
* Erwartetes Verhalten:
  VM wird gestoppt.
* Antwort: Default result status

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
time-measurement: true
parameter:
  retry-counter: <counter>
  migration-type: live | warm | offline
  rdma-migration: true | false
  pscom-hook-procs: <Anzahl der Prozesse>
  vcpu-map: [[<cpus>], [<cpus>], ...]
  swap-with:
    vm-name: <vm name>
    pscom-hook-procs: <Anzahl der Prozesse>
    vcpu-map: [[<cpus>], [<cpus>], ...]
```
* time-measurement: Gibt Informationen über die Dauer einzelner Phasen im result zurück. (Optional)
* pscom-hook-procs: Anzahl der Prozesse deren pscom Schicht unterbrochen werden muss. (Optional)
* vcpu-map: Ermöglicht die Neuzuordnung von VCPUs zu CPUs auf dem Zielsystem. Siehe [CPU Repin](#cpu-repin). (Optional)
* swap-with: Ermöglicht zwei VMs zu tauschen. Hier kann für die zweite VM ebenfalls optional pscom-hook-procs und vcpu-map angegeben werden. Die VM, welche unter swap-with angegeben wird, muss auf dem "destination"-Host laufen.
* Erwartetes Verhalten:
  VM wird vom Migrationsframework gestartet und anschließend wird eine
  entsprechende Statusinformation über den 'scheduler' channel gechickt.
* Antwort: Default result status


#### Evacuate node
Request from external instance (e.g., the scheduler) to evacuate the respective
computing node.
* topic: fast/migfra/\<hostname\>/task
* Payload

```
host: <string>
task: evacuate node
id: <uuid>
time-measurement: true | false
destinations:
  - <destinationhostname>
  - ...
parameter:
  retry-counter: <counter>
  mode: auto | compact | scatter
  migration-type: live | warm | offline
  rdma-migration: true | false
  overbooking: true | false
  pscom-hook-procs: <amount of pscom processes>
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
* vcpu-map enthält die Zuordnung von VCPUs zu CPUs.
Bsp. Zuordnung von VCPU 0 zu CPUs 4, 1 zu 5, usw.:
```
vcpu-map: [[4],[5],[6],[7]]
```
Bsp. Zuordnung von VCPUs 0-4 zu CPUs 4-7:
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
    status: success | error
    details: <string>
    process-id: <process id of the vm>
  - vm-name: <vm-hostname>
    status: success | error
    process-id: <process id of the vm>
  - ..
```
* details: Ermöglicht detailierte Fehlerinformationen zurückzugeben.
* Erwartetes Verhalten:
  Der für den Knoten zuständige Scheduler empfängt die Nachricht und
  startet die Anwendung in der VM.
* Implementierung:
  Über mqtt_publish in Startup Skript der VM. Hierbei gibt es die
  folgenden Möglichkeiten:
	1. VM sendet an migfra "vm ready", migfra sendet dies gebündelt an
	   scheduler mit "vm started"
	2. Migfra wartet bis alle VMs der task "start vm" per SSH erreichbar
	   sind und schickt gebündelt eine Liste der Stati an den zuständigen
	   scheduler.

#### Domain stopped
This message is emitted once the domain is shutdown successfully.
* topic: fast/migfra/\<hostname\>/result
* Payload

```
result: vm stopped
id: <uuid>
list:
  - vm-name: <vm-hostname>
    status: success | error
    details: <string>
  - vm-name: <vm-hostname>
    status: success | error
  - ..
```

* details: Ermöglicht detailierte Fehlerinformationen zurückzugeben.
* Erwartetes Verhalten:
  VM aufräumen? Log files für den Nutzer rauskopieren?

#### Domain suspended
This message is emitted once the domain is suspended successfully.
* topic: fast/migfra/\<hostname\>/result
* Payload

```
result: vm suspended
id: <uuid>
list:
  - vm-name: <vm-hostname>
    status: success | error
    details: <string>
  - vm-name: <vm-hostname>
    status: success | error
  - ..
```

* details: Ermöglicht detailierte Fehlerinformationen zurückzugeben.

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

* details: Ermöglicht detailierte Fehlerinformationen zurückzugeben.



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
* details: Ermöglicht detailierte Fehlerinformationen oder bei "success" die Anzahl der Versuche zurückzugeben.
* time-measurement: Falls Zeitmessungen im task aktiviert wurden, wird hier eine Liste von Tags mit Zeitdauern zurückgegeben.
* Erwartetes Verhalten:
  Scheduler markiert ursprüngliche Ressource als frei.

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
* details: Ermöglicht detailierte Fehlerinformationen zurückzugeben.

#### Shutdown connections
This message requests the pscom layer to execute the S/R protocol for all
non-migratable connections.
* topic: fast/pscom/\<hostname\>/\<pid\>/request
* Payload

```
task: suspend
```

* Erwartetes Verhalten:
  pscom faehr suspend-Protokoll ab

#### Reconnect connections
This message requests the pscom-layer to release the previously shut down
connections.
* topic: fast/pscom/\<hostname\>/\<pid\>/request
* Payload

```
task: resume
```

* Erwartetes Verhalten:
  pscom gibt Verbindungen frei
