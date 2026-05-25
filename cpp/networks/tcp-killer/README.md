# TCP Connection Tracker & Killer
A utility for diagnosing network problems on Linux. Parses `/proc/net/tcp`, shows active connections, and can forcefully close them by sending RST packets.  
## Features
View active TCP connections (similar to `netstat -t` / `ss -t`)  
Filter by PID, port, and connection status  
Real-time mode (`--watch`)  
Kill a connection by sending a RST packet  
Bash wrapper with convenient options  
Docker support  
## Build
```bash
make
