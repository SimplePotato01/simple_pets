#include <stdio.h>
#include <malloc.h>
#include <unistd.h>

int main(void){
	printf("%d\n",getpid());	
	sleep(60);
	return 0;
}
