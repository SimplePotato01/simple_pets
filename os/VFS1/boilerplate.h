#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>	/* Definition of AT_* constants */
#include <sys/stat.h>	/* stat - file status */
#include <dirent.h>

#define BUFFER_SIZE 4096
#define DEBUG 0

int reverse_string(const char *src, char *dst);
int is_regular_file(const char *path);
int check_existence(const char *src_dir); 
int create_dir(const char *target_dir_name, mode_t mode);
int get_file_permissions(const char *path, mode_t *mode);
int reverse_and_copy_file(const char *src_path, const char *dst_path);
int process_directory(const char *src_dir);
