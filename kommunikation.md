Umsetzung der Schnittstellen mit MQTT
=====================================

Grundlagen
----------

### Annahmen
* Basiert auf schnittstellen.md von <datum>
* Scheduler kennt die Hostnamen aller knoten.


### Aufbau einer MQTT Nachricht
Eine MQTT Nachricht besteht aus zwei Teilen

* Topic
* Payload in YAML


### Ablauf der Kommunikation
Grundsätzlich ist das ein einfaches Publisher-Subscriber Pattern.
Hier eine grobe Übersicht.

1. Ein Teilnehmer verbinden sich mit einem Server.
  * Ein Teilnehmer melden sich für N Topics an.
  * Der Server schickt alle Nachrichten für die sich ein Teilnehmer angemeldet
    hat.

2. Ein Teilnehmer meldet sich ab.

### Diskussion
* Eine msg pro gestartete VM oder eine msg mit allen gestarteten VMs an 
  Scheduler? (siehe Kommentar unter "vm started")
* Einheitlicher result status: "status: success | error" (Eventuell "details: 
  <error description>" hinzufügen für mehr Infos.)
* Einheitlicher task/result Name: "task: <start|stop|migrate> vm", "result: vm 
  <started|stopped|migrated>" 

### Online YAML parser
http://yaml-online-parser.appspot.com/


MQTT Topics
-----------

### Struktur
```
fast
+-- migfra
|   +-- <hostname>
|   |   +-- task
|   |   +-- result
|   +-- ...
|   +-- <hostname>
+-- agent
    +-- <hostname>
    |   +-- status 
    |   +-- task 
    +-- ...
    +-- <hostname>
```

### Subscriber
* Scheduler
  * fast/agent/+/status
    Überwachung der Statusänderungen *aller* Agenten
  * fast/migfra/+/result
    Ergebnisse von Anfragen des Schedulers an eine der
    Migrationsframework-Instanzen
* Migration-Framework
  * fast/migfra/<hostname>/task
    Entgegennehmen von Anfragen des Schedulers
* Agent 
  * fast/migfra/<hostname>/result
    Änderungen im Schedule des betreffenden nodes (z.B. neuer Job) 
  * fast/agent/<hostname>/task
    Anfragen des Schedulers an den entsprechenden Agenten (z.B. start/stop
    monitoring)

### Publisher
* Scheduler
  * fast/migfra/<hostname>/task
    Starten, Stoppen und Migrieren von VMs
  * fast/agent/<hostname>/task
    starten/stoppen des Monitorings; Konfiguration der KPIs
* Migration-Framework
  * fast/migfra/<hostname>/result
    VM gestartet/gestoppt/migriert
* Agent
  * fast/agent/<hostname>/status
    Anmeldung des Agenten beim Scheduler

Nachrichtenformat
-----------------
Im Folgenden werden die Nachrichten, welche über die oben definierten Topics
verteilt werden, definiert. Hierbei handelt es sich jeweils um Nachrichten im 
YAML Format. Die unterschiedlichen Nachrichten sind nach ihrer Quelle sortiert.

### Scheduler
#### Agenten Initialisieren
Der Scheduler meldet sich beim Agenten und liefert eine initiale Konfiguration.
* topic: fast/agent/<hostname>/task 
* Payload
  ```
  task: init agent
  KPI:
    categories:
      - energy consumption: <energy>
      - compute intensity: <high,medium,low>
      - IO intensity: <high,medium,low>
      - communication intensity (network): <high,medium,low>
      - expected runtime: <high,medium,low>
      - compute device: <CPU,GPU>
      - dependencies: <next_phase>
    repeat: <number in seconds how often the KPIs are reported>
  ```
* Erwartetes Verhalten:
  Der Agent merkt sich die entsprechende Konfiguration und reagiert
  entsprechend. Der für den Knoten zuständige Scheduler empfängt die 
  Nachrichten über das angegebene Topic.


#### Monitoring/Tuning beenden
Der Agent soll aufhören die Anwendung zu überwachen.
* topic: fast/agent/<hostname>/task 
* Payload
  ```
  task: stop monitoring
  job-description:
    job-id: <job id>
    process-id: <process id of the vm>
  ```
* Erwartetes Verhalten:
  Agent hört auf den Prozess zu überwachen.

#### VMs starten
Anfrage des Schedulers an die entsprechende Migrationsframework-Instanz
eine oder mehrere VMs zu starten.
Diskussion: VM vorbereiten in Scheduler Skripten oder von Migfra?
* topic: fast/migfra/<hostname>/task 
* Payload
  ```
  host: <string>  
  task: start vm
  vm-configurations:
    - name: <string>
      vcpus: #CPU
      memory: #byte
      job-id: <job id>
    - name: <string>
      vcpus: #CPU
      memory: #byte
      job-id: <job id>
    - ..
  ```
* Erwartetes Verhalten:
  VMs werden auf dem entsprechenden Host gestartet.

#### VM stoppen
Annahme: VM wird gestoppt wenn die Anwendung fertig ist / beendet werden soll.
* topic: fast/migfra/<hostname>/task 
* Payload
  ```
  host: <string>  
  task: stop vm
  list:
    - vm-name: <vm name>
    - vm-name: <vm name>
    - ..
  ```
* Erwartetes Verhalten:
  VM wird gestoppt.

#### Migration starten
Diese Nachricht informiert die zuständige Migrationsframework-Instanz darüber,
dass die Anwendung vom Quellknoten auf den Zielknoten migriert werden soll.
* topic: fast/migfra/<hostname>/task 
* Payload
  ```
  host: <string>  
  task: migrate vm
  vm-name: <vm name>
  destination: <destination hostname>
  parameter:
    live-migration: true | false
  ```

* Erwartetes Verhalten:
  VM wird vom Migrationsframework gestartet und anschließend wird eine
  entsprechende Statusinformation über den 'scheduler' channel gechickt.

### Migration-Framework
#### VM gestartet 
Nachdem die VM gestartet ist und bereit ist eine Anwendung auszuführen
informiert die entsprechende Migrationsframework-Instanz den Schduler darüber.
* topic: fast/migfra/<hostname>/status 
* Payload:
  ```
  scheduler: <hostname/global>
  result: vm started
  list:
    - vm-name: <vm-hostname>
      status: success | error
      process-id: <process id of the vm>
    - vm-name: <vm-hostname>
      status: success | error
      process-id: <process id of the vm>
    - ..
  ```
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

#### VM gestoppt
Informiert den zuständigen Scheduler, dass die VM gestoppt ist.
* topic: fast/migfra/<hostname>/status 
* Payload
  ```
  result: vm stopped
  list:
    - vm-name: <vm-hostname>
      status: success | error
    - vm-name: <vm-hostname>
      status: success | error
    - ..
  ```
* Erwartetes Verhalten:
  VM aufräumen? Log files für den Nutzer rauskopieren?

#### Migration abgeschlossen
Meldung an den Scheduler dass die Migration fertig ist.
* topic: fast/migfra/<hostname>/status 
* Payload
  ```
  result: vm migrated
  vm-name: <vm name>
  status: success | error
  process-id: <process id of the vm>
  ```
* Erwartetes Verhalten:
  Scheduler markiert ursprüngliche Ressource als frei.

### Agent 
#### Agenten anmelden 
Knoten wird gestartet. Meldet und meldet sich beim Scheduler an.
* topic: fast/agent/<hostname>/status
* payload:
  ```
  task: init
  source: <hostname>
  ```
* Erwartetes Verhalten:
  Der für den Knoten zuständige Scheduler empfängt die Nachricht und 
  nimmt den Knoten in sein Scheduling mit auf.
* Implementierung:
  Agent wird automatisch bei Systemstart gestartet und verschickt die
  o.g. Nachricht.
