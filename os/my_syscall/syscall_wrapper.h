#include <stdint.h>
#include <stddef.h>

#if defined(__x86_64__)
	// x86 (64-bit)
	#define SYSCALL_INSTR "syscall"
	#define SYS_EXIT  60
	#define SYS_WRITE 1
	#define SYS_READ  0

	static inline long my_syscall(long number,
	long arg1, long arg2, long arg3,
	long arg4, long arg5, long arg6) {
		long result;
		
		__asm__ volatile (
		"mov %1, %%rax\n"        // номер системного вызова
		"mov %2, %%rdi\n"        // arg1
		"mov %3, %%rsi\n"        // arg2
		"mov %4, %%rdx\n"        // arg3
		"mov %5, %%r10\n"        // arg4
		"mov %6, %%r8\n"         // arg5
		"mov %7, %%r9\n"         // arg6
		SYSCALL_INSTR "\n"
		"mov %%rax, %0\n"
		: "=r"(result)
		: "r"(number), "r"(arg1), "r"(arg2), "r"(arg3),
		"r"(arg4), "r"(arg5), "r"(arg6)
		: "%rax", "%rdi", "%rsi", "%rdx", "%r10", "%r8", "%r9", "memory"
		);

		return result;
	}

#elif defined(__i386__)
	// x86 (32-bit)
	#define SYSCALL_INSTR "int $0x80"
	#define SYS_EXIT  1
	#define SYS_WRITE 4
	#define SYS_READ  3
    
	static inline long my_syscall(long number,
	long arg1, long arg2, long arg3,
	long arg4, long arg5, long arg6) {
		long result;
        
		__asm__ volatile (
		"mov %1, %%eax\n"        // номер системного вызова
		"mov %2, %%ebx\n"        // arg1
		"mov %3, %%ecx\n"        // arg2
		"mov %4, %%edx\n"        // arg3
		"mov %5, %%esi\n"        // arg4
		"mov %6, %%edi\n"        // arg5
		"mov %7, %%ebp\n"        // arg6
		SYSCALL_INSTR "\n"
		"mov %%eax, %0\n"
		: "=r"(result)
		: "r"(number), "r"(arg1), "r"(arg2), "r"(arg3),
		"r"(arg4), "r"(arg5), "r"(arg6)
		: "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "%ebp", "memory"
		);

		return result;
	}

#elif defined(__arm__)
	// ARM (32-bit)
	#define SYSCALL_INSTR "swi 0"
	#define SYS_EXIT  1
	#define SYS_WRITE 4
	#define SYS_READ  3
    
	static inline long my_syscall(long number,
	long arg1, long arg2, long arg3,
	long arg4, long arg5, long arg6) {
		register long r7 asm("r7") = number;
		register long r0 asm("r0") = arg1;
		register long r1 asm("r1") = arg2;
		register long r2 asm("r2") = arg3;
		register long r3 asm("r3") = arg4;
		register long r4 asm("r4") = arg5;
		register long r5 asm("r5") = arg6;
        
		__asm__ volatile (
		SYSCALL_INSTR "\n"
		: "+r"(r0)
		: "r"(r7), "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
		: "memory"
		);

		return r0;
	}

#elif defined(__aarch64__)
    // ARM64
    #define SYSCALL_INSTR "svc #0"
    #define SYS_EXIT  93
    #define SYS_WRITE 64
    #define SYS_READ  63
    
    static inline long my_syscall(long number,
                                  long arg1, long arg2, long arg3,
                                  long arg4, long arg5, long arg6) {
        long result;
        
        register long x8 asm("x8") = number;
        register long x0 asm("x0") = arg1;
        register long x1 asm("x1") = arg2;
        register long x2 asm("x2") = arg3;
        register long x3 asm("x3") = arg4;
        register long x4 asm("x4") = arg5;
        register long x5 asm("x5") = arg6;
        
        __asm__ volatile (
            SYSCALL_INSTR "\n"
            : "+r"(x0)
            : "r"(x8), "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
            : "memory"
        );
        
        result = x0;
        return result;
    }

#else
	#error "No idea about architecture due my own man 2"
#endif

static inline long my_syscall0(long number) {
	return my_syscall(number, 0, 0, 0, 0, 0, 0);
}

static inline long my_syscall1(long number, long arg1) {
	return my_syscall(number, arg1, 0, 0, 0, 0, 0);
}

static inline long my_syscall2(long number, long arg1, long arg2) {
	return my_syscall(number, arg1, arg2, 0, 0, 0, 0);
}

static inline long my_syscall3(long number, long arg1, long arg2, long arg3) {
	return my_syscall(number, arg1, arg2, arg3, 0, 0, 0);
}

static inline long my_syscall4(long number, long arg1, long arg2, long arg3, long arg4) {
	return my_syscall(number, arg1, arg2, arg3, arg4, 0, 0);
}

static inline long my_syscall5(long number, long arg1, long arg2, long arg3, long arg4, long arg5) {
	return my_syscall(number, arg1, arg2, arg3, arg4, arg5, 0);
}
