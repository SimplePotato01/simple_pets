#include "syscall_wrapper.h"

size_t my_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

void my_puts(const char *s) {
    size_t len = my_strlen(s);
    my_syscall3(SYS_WRITE, 1, (long)s, len);
}

int main() {
    const char *message = "Hello world via my own syscall!\n";
    my_puts(message);
    
    // демонстрация работы 'со всем'
    my_syscall1(SYS_EXIT, 0);
}
