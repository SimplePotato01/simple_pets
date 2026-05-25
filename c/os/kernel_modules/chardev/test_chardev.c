#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVICE_PATH "/dev/mychardev"

int main() {
    int fd;
    char write_buf[] = "Hello from user space! This is a test message for my kernel module!";
    char read_buf[256];
    ssize_t bytes_written, bytes_read;
    
    // 1. Открываем устройство
    printf("Открываю %s...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Ошибка open");
        if (errno == EACCES) {
            printf("Совет: запустите программу с sudo или выполните: sudo chmod 666 /dev/mychardev\n");
        }
        return 1;
    }
    printf("Устройство открыто, fd=%d\n", fd);
    
    // 2. Пишем в устройство
    printf("\nПишу: \"%s\"\n", write_buf);
    bytes_written = write(fd, write_buf, strlen(write_buf));
    if (bytes_written < 0) {
        perror("Ошибка write");
        close(fd);
        return 1;
    }
    printf("Записано %ld байт\n", bytes_written);
    
    // 3. Сбрасываем позицию (lseek в начало)
    lseek(fd, 0, SEEK_SET);
    
    // 4. Читаем из устройства
    memset(read_buf, 0, sizeof(read_buf));
    printf("\nЧитаю...\n");
    bytes_read = read(fd, read_buf, sizeof(read_buf) - 1);
    if (bytes_read < 0) {
        perror("Ошибка read");
        close(fd);
        return 1;
    }
    printf("Прочитано %ld байт: \"%s\"\n", bytes_read, read_buf);
    
    // 5. Дополнительный тест: множественные операции
    printf("\n=== Дополнительный тест ===\n");
    
    // Записываем вторую строку
    char write_buf2[] = "Second message!";
    printf("Пишем вторую строку: \"%s\"\n", write_buf2);
    lseek(fd, 0, SEEK_END);  // В конец
    bytes_written = write(fd, write_buf2, strlen(write_buf2));
    printf("Записано %ld байт\n", bytes_written);
    
    // Читаем всё
    lseek(fd, 0, SEEK_SET);
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = read(fd, read_buf, sizeof(read_buf) - 1);
    printf("Читаем всё: %ld байт: \"%s\"\n", bytes_read, read_buf);
    
    // 6. Закрываем устройство
    close(fd);
    printf("\nУстройство закрыто\n");
    
    return 0;
}
