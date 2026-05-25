# VLAN-aware bridge with STP
4 virtual ports (veth-pairs)  
Support for 802.1Q (vlan 10,20,30)  
Implementation of Rapid Spanning Tree (only states: DISCARDING, LEARNING, FORWARDING)  
Sending/receiving BPDU every 2 seconds  
Logging of state changes  
## Build
```bash
make
