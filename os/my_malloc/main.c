#include "heap.h"
#include <stdio.h>
#include <string.h>

#define RED "\033[31m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"
#define PURP "\033[35m"
#define RESET "\033[0m"

int main() {
    	int test_result_flag = 0;
    	// Тест 1: Базовое выделение памяти
    	printf(BLUE "Тест 1: Базовое выделение памяти\n" RESET);
    	char *str1 = my_malloc(50);
    	if (str1) {
        	strcpy(str1, "Hello from the custom heap!");
        	printf("Allocated string: 	'%s'	Address: 	%p", str1, str1);
    	} else ++test_result_flag;
    	dump_heap();
    	heap_stats();
    
    	// Тест 2: Множественные выделения
    	printf(BLUE "\nТест 2: Множественные выделения\n" RESET);
    	int *arr1 = my_malloc(10 * sizeof(int));
    	double *arr2 = my_malloc(5 * sizeof(double));
    	char *str2 = my_malloc(100);
    
    	if (arr1 && arr2 && str2) {
        	for (int i = 0; i < 10; i++) arr1[i] = i * i;
        	for (int i = 0; i < 5; i++) arr2[i] = i * 3.14;
        	strcpy(str2, "Another string in the custom heap");
        
        	printf("Array1[5] = %d\n", arr1[5]);
        	printf("Array2[2] = %.2f\n", arr2[2]);
        	printf("String2: '%s'\n", str2);
    	}
    	dump_heap();
    	heap_stats();
    
    	// Тест 3: Освобождение памяти
    	printf(BLUE "\nТест 3: Освобождение памяти\n" RESET);
    	my_free(str1);
    	printf("Freed str1\n");
    	dump_heap();
    	heap_stats();
    
    	// Тест 4: Проверка на некорректный указатель
    	printf(BLUE "\nТест 4: Проверка на некорректный указатель\n" RESET);
    	char invalid_ptr[10];
    	my_free(invalid_ptr);  // Должен выдать ошибку
    	my_free((void*)0x12345678);  // Случайный указатель
    	printf("must be 2 errors\n\n");
    
    	// Тест 5: Освобождение и повторное выделение
    	printf(BLUE "\nТест 5: Освобождение и повторное выделение\n" RESET);
    	char *str3 = my_malloc(60);
    	if (str3) {
        	strcpy(str3, "Reused memory!");
        	printf("New string: '%s'\n", str3);
    	}
    	dump_heap();
    	heap_stats();
    
    	// Тест 6: Освобождение всех блоков
    	printf(BLUE "\nТест 6: Освобождение всех блоков\n" RESET);
    	my_free(arr1);
    	my_free(arr2);
    	my_free(str2);
    	my_free(str3);
    	dump_heap();
    	heap_stats();
    
    	// Тест 7: Попытка выделить больше памяти, чем доступно
    	printf(BLUE "\nТест 7: Попытка выделить больше памяти, чем доступно\n" RESET);
    	void *big_block = my_malloc(get_heap_size() + 1000);
    	if (big_block == NULL) printf("Successfully failed to allocate too large block\n");
    	else ++test_result_flag;

    	// Тест 8: Выделение нулевого размера
    	printf(BLUE "\nТест 8: Выделение нулевого размера\n" RESET);
    	void *null_ptr = my_malloc(0);
    	if (null_ptr == NULL) printf("Successfully handled zero size allocation\n");
   	else ++test_result_flag;

    	// Тест 9: Проверка на double free
    	printf(BLUE "\nТест 9: Проверка на double free\n" RESET);
    	char *test_ptr = my_malloc(100);
    	my_free(test_ptr);
    	my_free(test_ptr);
    

    	if (test_result_flag == 0) printf(GREEN "\nAll tests completed successfully!\n" RESET);
    	else printf(RED "broken tests: %d\n" RESET, test_result_flag);
    	return 0;
}
