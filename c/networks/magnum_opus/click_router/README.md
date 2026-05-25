# configurable processing pipeline
# Elements:
FromDevice(eth0) -> Classifier(ether proto 0x0800) ->  
IPLookup(10.0.0.0/24 -> 1, default -> 2) ->  
Queue(100) -> ToDevice(eth1)  
# Architecture:
Thread pools (one element per thread)  
Ring buffer between elements  
Configuration via a text file  
## Build
```bash
make
