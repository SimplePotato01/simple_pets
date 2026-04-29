#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

// Структура одной записи pagemap-64 бита
typedef struct {
    	uint64_t pfn : 54;          // номер физического кадра (Page Frame Number)
    	uint64_t soft_dirty : 1;    // флаг soft-dirty
    	uint64_t file_page : 1;     // страница из файла (если 0 то анонимная)
    	uint64_t swapped : 1;       // страница в swap
    	uint64_t present : 1;       // страница в памяти
} pagemap_entry_t;

// Функция для вывода битовой маски в человекочитаемом виде. Ужас какой.
void print_entry_info(uint64_t entry, unsigned long vaddr) {
    	pagemap_entry_t *pe = (pagemap_entry_t *)&entry;
    
    	// Приводим битовые поля к обычным типам для printf
    	uint64_t pfn = pe->pfn;
    	uint64_t physical_addr = pe->pfn << 12;
    	
    	printf("  Вирт. адрес: 0x%016lx\n", vaddr);
    	printf("  Значение:     0x%016lx\n", entry);
    
    	printf("  Поля:\n");
    	printf("    - PFN (номер физ. кадра): 0x%lx (%lu)\n", pfn, pfn);
    	printf("    - Present (в памяти):     %s\n", pe->present ? "Да" : "Нет");
    	printf("    - Swapped (в swap):       %s\n", pe->swapped ? "Да" : "Нет");
    	printf("    - File page (файловая):   %s\n", pe->file_page ? "Да" : "Нет");
    	printf("    - Soft dirty:             %s\n", pe->soft_dirty ? "Да" : "Нет");
    
    	if (pe->present) {
        	printf("    - Физ. адрес:           0x%lx\n", physical_addr);
    	} else if (pe->swapped) {
        	printf("    - Swap-тип/оффсет:      %lu\n", pfn);
    	}
    
    	printf("\n");
}

long get_page_size(void) {
    	long page_size = sysconf(_SC_PAGESIZE);
    	if (page_size == -1) {
        	perror("sysconf(_SC_PAGESIZE)");
        	return 4096;
    	}
    	return page_size;
}

unsigned long get_num_pagemap_entries(pid_t pid) {
    	char maps_path[256];
    	snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    
    	FILE *fp = fopen(maps_path, "r");
    	if (!fp) {
        	perror("fopen maps");
        	return 0;
    	}
    
    	unsigned long max_addr = 0;
    	char line[512];
    
    	while (fgets(line, sizeof(line), fp)) {
        	unsigned long start, end;
        	if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
            		if (end > max_addr) max_addr = end;
        	}
    	}
    
    	fclose(fp);
    
    	if (max_addr == 0) return 0;
    
    	long page_size = get_page_size();
    	return (max_addr + page_size - 1) / page_size;
}

int main(int argc, char *argv[]) {
    	if (argc != 2) {
        	fprintf(stderr, "Использование: %s <PID>\n", argv[0]);
        	return 1;
    	}
    
    	pid_t pid = atoi(argv[1]);
    	if (pid <= 0) {
        	fprintf(stderr, "Неверный PID\n");
        	return 1;
    	}
    
    	char pagemap_path[256];
    	snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%d/pagemap", pid);
    
    	if (access(pagemap_path, R_OK) != 0) {
        	perror("Доступ к pagemap:\n");
        	return 1;
    	}
    
    	int fd = open(pagemap_path, O_RDONLY);
    	if (fd < 0) {
        	perror("open pagemap:\n");
        	return 1;
    	}
    
    	long page_size = get_page_size();
    	printf("Размер страницы: %ld байт\n", page_size);
    	printf("PID процесса: %d\n", pid);
    	printf("Путь: %s\n", pagemap_path);
    	printf("========================================\n\n");
    
    	char maps_path[256];
    	snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    
    	FILE *maps_fp = fopen(maps_path, "r");
    	if (!maps_fp) {
        	perror("fopen maps");
        	close(fd);
        	return 1;
    	}
    
    	char line[512];
    	int entry_count = 0;
    
    	printf("Структура страниц памяти:\n\n");
    
    	// Чтение каждой области памяти и соответствующие записи pagemap
    	while (fgets(line, sizeof(line), maps_fp)) {
        	unsigned long start, end;
        	char perms[5];
        	char offset[16];
        	char dev[8];
        	unsigned long inode;
        	char path[256] = "";
        
        	sscanf(line, "%lx-%lx %4s %s %s %lu %255[^\n]", &start, &end, perms, offset, dev, &inode, path);
        
        	// Скип неподходящих областей (если нужно)
        	if (strlen(path) == 0) strcpy(path, "[anonymous]");
        
        	printf("Область: 0x%016lx-0x%016lx [%s] %s\n", start, end, perms, path);
        
        	// Вычисляем начальную позицию в pagemap для этой области
        	unsigned long start_pfn = start / page_size;
        	off_t pagemap_offset = start_pfn * sizeof(uint64_t);
        
        	// Перемещаемся в нужную позицию в файле pagemap
        	if (lseek(fd, pagemap_offset, SEEK_SET) == (off_t)-1) {
            		perror("lseek");
            		continue;
        	}
        
        	// Количество страниц в этой области
        	unsigned long num_pages = (end - start) / page_size;
        
        	// Читаем записи pagemap для каждой страницы
        	for (unsigned long i = 0; i < num_pages; i++) {
            		uint64_t entry;
            		ssize_t bytes_read = read(fd, &entry, sizeof(entry));
            
            		if (bytes_read != sizeof(entry)) {
                		if (bytes_read == 0) break;
                		perror("read");
                		break;
            		}
            
            		unsigned long vaddr = start + i * page_size;
            
            		// Выводим только "интересные страницы" (present или swapped)
            		// Можно закомментить чтобы видеть все страницы
            		pagemap_entry_t *pe = (pagemap_entry_t *)&entry;
            		if (pe->present || pe->swapped) {
                		printf("  Страница %lu (смещение 0x%lx):\n", i, i * page_size);
                		print_entry_info(entry, vaddr);
                		entry_count++;
            		}
        	}
        
        	printf("	---Конец области---\n\n");
    	}
    
 	fclose(maps_fp);
 	close(fd);
    
    	printf("========================================\n");
    	printf("Всего записей со страницами: %d\n", entry_count);
    
    	return 0;
}
