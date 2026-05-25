# Micro-TCP-stack
it implements (without libc network):  
ARP (cache + requests)  
IP (fragmentation/defragmentation)  
ICMP (responds to ping)  
TCP (3WH, SEQ/ACK, retransmit, RST, FIN)  
Small buffer (64KB per connection)  
Works via raw socket  
## Build
```bash
make
