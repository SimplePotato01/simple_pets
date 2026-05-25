#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <ctime>

struct TcpConnection {
    std::string local_addr;
    uint16_t local_port;
    std::string remote_addr;
    uint16_t remote_port;
    std::string state;
    uid_t uid;
    pid_t pid;
    std::string process_name;
    uint64_t inode;
};

bool parse_hex_ip_port(const std::string& hex, std::string& ip, uint16_t& port) {
    if (hex.length() < 8) return false;
    
    uint32_t addr;
    port = (uint16_t)strtol(hex.substr(4,4).c_str(), NULL, 16);
    addr = (uint32_t)strtol(hex.substr(0,8).c_str(), NULL, 16);
    
    struct in_addr in;
    in.s_addr = addr;
    ip = inet_ntoa(in);
    return true;
}

std::string get_process_name(pid_t pid) {
    std::ifstream comm("/proc/" + std::to_string(pid) + "/comm");
    std::string name;
    getline(comm, name);
    return name;
}

pid_t find_pid_by_inode(uint64_t inode) {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) continue;
        
        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        
        std::string fd_path = "/proc/" + std::string(entry->d_name) + "/fd";
        DIR* fd_dir = opendir(fd_path.c_str());
        if (!fd_dir) continue;
        
        struct dirent* fd_entry;
        while ((fd_entry = readdir(fd_dir)) != NULL) {
            if (fd_entry->d_type != DT_LNK) continue;
            
            std::string link_path = fd_path + "/" + fd_entry->d_name;
            char buf[256];
            ssize_t len = readlink(link_path.c_str(), buf, sizeof(buf)-1);
            if (len > 0) {
                buf[len] = '\0';
                std::string link(buf);
                size_t pos = link.find("socket:[");
                if (pos != std::string::npos) {
                    uint64_t found_inode = stoull(link.substr(pos + 8, link.length() - pos - 9));
                    if (found_inode == inode) {
                        closedir(fd_dir);
                        closedir(dir);
                        return pid;
                    }
                }
            }
        }
        closedir(fd_dir);
    }
    closedir(dir);
    return -1;
}

std::vector<TcpConnection> parse_tcp_connections(const std::string& proc_path) {
    std::vector<TcpConnection> connections;
    std::ifstream file(proc_path);
    if (!file.is_open()) return connections;
    
    std::string line;
    getline(file, line); // Skipping Header
    
    while (getline(file, line)) {
        TcpConnection conn;
        std::istringstream iss(line);
        std::string token;
        
        // Format: sl local_address rem_address st tx_queue rx_queue tr tm->when retrnsmt uid timeout inode
        std::vector<std::string> tokens;
        while (iss >> token) tokens.push_back(token);
        
        if (tokens.size() < 10) continue;
        
        // local_address и remote_address
        if (!parse_hex_ip_port(tokens[1], conn.local_addr, conn.local_port)) continue;
        if (!parse_hex_ip_port(tokens[2], conn.remote_addr, conn.remote_port)) continue;
        
        // State (hex -> string)
        int state_hex = strtol(tokens[3].c_str(), NULL, 16);
        switch(state_hex) {
            case 1: conn.state = "ESTABLISHED"; break;
            case 2: conn.state = "SYN_SENT"; break;
            case 3: conn.state = "SYN_RECV"; break;
            case 4: conn.state = "FIN_WAIT1"; break;
            case 5: conn.state = "FIN_WAIT2"; break;
            case 6: conn.state = "TIME_WAIT"; break;
            case 7: conn.state = "CLOSE"; break;
            case 8: conn.state = "CLOSE_WAIT"; break;
            case 9: conn.state = "LAST_ACK"; break;
            case 10: conn.state = "LISTEN"; break;
            case 11: conn.state = "CLOSING"; break;
            default: conn.state = "UNKNOWN";
        }
        
        conn.uid = (tokens.size() > 7) ? stoul(tokens[7]) : 0;
        conn.inode = (tokens.size() > 9) ? stoull(tokens[9]) : 0;
        conn.pid = find_pid_by_inode(conn.inode);
        
        if (conn.pid > 0) {
            conn.process_name = get_process_name(conn.pid);
        } else {
            conn.process_name = "unknown";
        }
        
        connections.push_back(conn);
    }
    return connections;
}

unsigned short tcp_checksum(unsigned short *ptr, int nbytes) {
    register long sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1)
        sum += *(unsigned char*)ptr;
    
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

void send_rst_packet(const std::string& src_ip, uint16_t src_port,
                     const std::string& dst_ip, uint16_t dst_port,
                     uint32_t seq_num = 0) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("Socket creation failed. Run with sudo!");
        return;
    }
    
    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt failed");
        close(sock);
        return;
    }
    
    // IP
    struct iphdr ip;
    ip.ihl = 5;
    ip.version = 4;
    ip.tos = 0;
    ip.tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    ip.id = htons(rand() % 65535);
    ip.frag_off = 0;
    ip.ttl = 255;
    ip.protocol = IPPROTO_TCP;
    ip.check = 0;
    ip.saddr = inet_addr(src_ip.c_str());
    ip.daddr = inet_addr(dst_ip.c_str());
    
    // TCP
    struct tcphdr tcp;
    tcp.source = htons(src_port);
    tcp.dest = htons(dst_port);
    tcp.seq = htonl(seq_num);
    tcp.ack_seq = 0;
    tcp.doff = 5;
    tcp.res1 = 0;
    tcp.res2 = 0;
    tcp.fin = 0;
    tcp.syn = 0;
    tcp.rst = 1;
    tcp.psh = 0;
    tcp.ack = 0;
    tcp.urg = 0;
    tcp.window = htons(0);
    tcp.check = 0;
    tcp.urg_ptr = 0;
    
    // Fake-Header for a checksum
    struct pseudo_header {
        uint32_t source_address;
        uint32_t dest_address;
        uint8_t placeholder;
        uint8_t protocol;
        uint16_t tcp_length;
    } pseudo;
    
    pseudo.source_address = ip.saddr;
    pseudo.dest_address = ip.daddr;
    pseudo.placeholder = 0;
    pseudo.protocol = IPPROTO_TCP;
    pseudo.tcp_length = htons(sizeof(tcp));
    
    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
    unsigned char *pseudogram = new unsigned char[psize];
    
    memcpy(pseudogram, (char*)&pseudo, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), &tcp, sizeof(struct tcphdr));
    
    tcp.check = tcp_checksum((unsigned short*)pseudogram, psize);
    delete[] pseudogram;
    
    // Final packet 
    char packet[4096];
    memcpy(packet, &ip, sizeof(ip));
    memcpy(packet + sizeof(ip), &tcp, sizeof(tcp));
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = tcp.dest;
    sin.sin_addr.s_addr = ip.daddr;
    
    if (sendto(sock, packet, ntohs(ip.tot_len), 0, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("Send failed");
    } else {
        std::cout << "RST packet sent to " << dst_ip << ":" << dst_port << std::endl;
    }
    close(sock);
}

void list_connections(const std::vector<TcpConnection>& conns,
                      pid_t filter_pid = 0,
                      uint16_t filter_port = 0,
                      const std::string& filter_state = "") {
    std::cout << std::left 
              << std::setw(8) << "PID"
              << std::setw(20) << "PROCESS"
              << std::setw(22) << "LOCAL"
              << std::setw(22) << "REMOTE"
              << std::setw(15) << "STATE"
              << std::endl;
    std::cout << std::string(87, '-') << std::endl;
    
    for (const auto& conn : conns) {
        if (filter_pid && conn.pid != filter_pid) continue;
        if (filter_port && conn.local_port != filter_port && conn.remote_port != filter_port) continue;
        if (!filter_state.empty() && conn.state != filter_state) continue;
        
        if (conn.pid > 0 || !filter_pid) {
            std::cout << std::left
                      << std::setw(8) << (conn.pid > 0 ? conn.pid : 0)
                      << std::setw(20) << (conn.process_name.length() > 19 ? conn.process_name.substr(0, 19) : conn.process_name)
                      << std::setw(22) << (conn.local_addr + ":" + std::to_string(conn.local_port))
                      << std::setw(22) << (conn.remote_addr + ":" + std::to_string(conn.remote_port))
                      << std::setw(15) << conn.state
                      << std::endl;
        }
    }
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "Options:\n"
              << "  --list                     List all TCP connections\n"
              << "  --pid <PID>                Filter by process ID\n"
              << "  --port <PORT>              Filter by port\n"
              << "  --state <STATE>            Filter by state (ESTABLISHED, LISTEN, etc.)\n"
              << "  --kill <LOCAL_IP:PORT> <REMOTE_IP:PORT>  Kill connection by sending RST\n"
              << "  --help                     Show this help\n"
              << "\nExamples:\n"
              << "  " << prog << " --list --pid 1234\n"
              << "  " << prog << " --list --port 80 --state ESTABLISHED\n"
              << "  " << prog << " --kill 192.168.1.100:54321 1.2.3.4:80\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string cmd = argv[1];
    
    if (cmd == "--help") {
        print_usage(argv[0]);
        return 0;
    }
    
    if (cmd == "--list") {
        pid_t filter_pid = 0;
        uint16_t filter_port = 0;
        std::string filter_state;
        
        for (int i = 2; i < argc; i += 2) {
            std::string opt = argv[i];
            if (opt == "--pid" && i+1 < argc) filter_pid = atoi(argv[i+1]);
            else if (opt == "--port" && i+1 < argc) filter_port = atoi(argv[i+1]);
            else if (opt == "--state" && i+1 < argc) filter_state = argv[i+1];
        }
        
        auto conns = parse_tcp_connections("/proc/net/tcp");
        auto conns6 = parse_tcp_connections("/proc/net/tcp6");
        conns.insert(conns.end(), conns6.begin(), conns6.end());
        
        list_connections(conns, filter_pid, filter_port, filter_state);
    }
    else if (cmd == "--kill" && argc >= 4) {
        std::string local_spec = argv[2];
        std::string remote_spec = argv[3];
        
        size_t colon1 = local_spec.find(':');
        size_t colon2 = remote_spec.find(':');
        
        if (colon1 == std::string::npos || colon2 == std::string::npos) {
            std::cerr << "Invalid format. Use IP:PORT\n";
            return 1;
        }
        
        std::string src_ip = local_spec.substr(0, colon1);
        uint16_t src_port = stoi(local_spec.substr(colon1+1));
        std::string dst_ip = remote_spec.substr(0, colon2);
        uint16_t dst_port = stoi(remote_spec.substr(colon2+1));
        
        send_rst_packet(src_ip, src_port, dst_ip, dst_port);
    }
    else {
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}
