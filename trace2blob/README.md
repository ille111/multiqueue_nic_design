# trace2blob

### Description
The esp32 does not provide much flash memory. To have the trace at hand the it is compressed and copied onto the flash as const char pointer.

trace2blob is used in the **OS-Configurable Multiqueue NIC for Real-Time IIoT Devices** project.

#### Usage
This script converts a trace from the NIC simulator. The trace csv file must be provided to `stdin` in order to get blob header to be put out on `stdout`.
```bash
cat trace.csv | ./trace2blob > blob.h
```
#### Test
The following command will output an example to the terminal:
```bash
echo -e "40, ['127.0.0.1', '127.0.0.2']\n65575, ['127.0.0.3', '127.0.0.4']" | ./trace2blob.sh
```