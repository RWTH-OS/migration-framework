### Example files
This folder contains a `migfra.conf` that sets lxctools as default driver and tcp as default transport.
Copy `migfra.conf` in the directory of the migfra executable or specifiy it with `migfra -c "<path>/migfra.conf"`

Also, messages to start, stop, and migrate a container are provided.

### Send messages:

```bash
mosquitto_pub -q 2 -t fast/migfra/<hostname>/task -f <message-file>
```

### Monitor sent messages:

```bash
mosquitto_sub -q 2 -t fast/migfra/#
```
