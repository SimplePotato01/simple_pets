#include <stdio.h>
#include <unistd.h>

int main(void){
	printf("%d\n",getpid());
	printf("printf of test_code_text\n");
	sleep(60);
	return 0;
}
