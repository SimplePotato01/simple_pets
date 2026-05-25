# Caching DNS proxy
Listens on UDP 53  
Parses DNS requests (with no libraries!)  
First checks local cache (hash table)  
If not - sends to 8.8.8.8  
Caches response with TTL  
Multithreaded (one thread per request)  
## Build
```bash
make
