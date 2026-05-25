#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <errno.h>

// Парсер имен сисколов
void load_syscall_names(const char *names[512]) {
    	FILE *f = fopen("/usr/include/asm/unistd_64.h", "r");
		if (!f) return;
    
    	char line[256];
    	while (fgets(line, sizeof(line), f)) {
        	int num;
        	char name[64];
        	// Searching for: #define __NR_read 0
        	if (sscanf(line, "#define __NR_%63s %d", name, &num) == 2) {
            		if (num >= 0 && num < 512) names[num] = strdup(name);
        	}
    	}
    	fclose(f);
}

const char *syscall_name(int num, const char *names[512]) {
    	if (num >= 0 && num < 512 && names[num])
        	return names[num];
    	return "???";
}

int main(int argc, char *argv[]) {
    	if (argc < 2) {
        	fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        	return 1;
    	}
    
    	const char *syscall_names[512] = {0};
    	load_syscall_names(syscall_names);
    
    	pid_t pid = fork();
    	if (pid == -1) {
        	perror("fork:\n");
        	return 1;
    	}
    
    	if (pid == 0) {
        	ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        	execvp(argv[1], argv + 1);
        	perror("execvp:\n");
        	return 1;
    	}
    
    	int status;
    	waitpid(pid, &status, 0);
    	ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD);
    
    	int call_count = 0;
    	int in_syscall = 0;
    	long syscall_nr;
   	
       	/* Перед выполнением - когда orig_rax содержит номер сискола
	*  После выполнения - когда rax содержит результат
	*/	
    	while (1) {
        	if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1) break;
        	waitpid(pid, &status, 0);
        
        	if (WIFEXITED(status) || WIFSIGNALED(status)) break;
        
        	struct user_regs_struct regs;
        	ptrace(PTRACE_GETREGS, pid, NULL, &regs);
        
        	if (!in_syscall) {
            		syscall_nr = regs.orig_rax;
            		printf("[%d] %s(", ++call_count, syscall_name(syscall_nr, syscall_names));
            		printf("0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx)\n",regs.rdi, regs.rsi, regs.rdx, regs.r10, regs.r8, regs.r9);
        	} else {
            		printf("    -> %lld (0x%llx)\n", (long long)regs.rax, (unsigned long long)regs.rax);
        	}
        	in_syscall = !in_syscall;
    	}
    
    	printf("\nTotal: %d syscalls\n", call_count);
    
    	for (int i = 0; i < 512; i++) {
        	if (syscall_names[i]) free((void*)syscall_names[i]);
    	}
    
    	return 0;
}
