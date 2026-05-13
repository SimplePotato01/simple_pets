#include "boilerplate.h"

int reverse_string(const char *src, char *dst) {
	if (src == NULL || dst == NULL) 
		return -1;
	int len = strlen(src);
	for (int i = 0; i < len; i++) {
        	dst[i] = src[len - 1 - i];
	}
	dst[len] = '\0';
	return len;
}

int is_regular_file(const char *path) {
    	struct stat statbuf;
    	if (stat(path, &statbuf) != 0) {
        	return 0;
    	}
    	return S_ISREG(statbuf.st_mode);
}

int check_existence(const char *source_dir){
    	struct stat statbuf;
        if (stat(source_dir, &statbuf) != 0) {
                perror("check_existence:\n");
                return -1;
     	}
	return 0;
}

int create_dir(const char *target_dir_name, mode_t mode){
        if (mkdir(target_dir_name, mode) == -1) {
		if (errno != EEXIST) {
                	perror("mkdir in create_dir return:\n");
                	return -1;
		}
	}
     	return 0;
}

int get_file_permissions(const char *path, mode_t *mode) {
	struct stat statbuf;
	if (stat(path, &statbuf) != 0){
		perror("get_file_permissions\n");
		return -1;
	}
	*mode = statbuf.st_mode & 07777; // права доступа это биты 0-11
	return 0;
}

int reverse_and_copy_file(const char *src_path, const char *dst_path) {
	//права
	mode_t src_mode;
	if(get_file_permissions(src_path, &src_mode) != 0){
		perror("reverse_and_copy_file\n");
		return -1;
	}
	
	// фд
	int src_fd = open(src_path, O_RDONLY);
	if (src_fd == -1) {
		perror("reverse_and_copy_file:\n");
		return -1;
	}
	
	// размер
	off_t file_size = lseek(src_fd, 0, SEEK_END);
	if (file_size == -1) {
		perror("reverse_and_copy_file:\n");
		close(src_fd);
		return -1;
	}

	// создание файла и приведение прав
	int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, src_mode);
	if (dst_fd == -1) {
		perror("reverse_and_copy_file:\n");
                close(src_fd);
                return -1;
	}

	// проверка соотв. прав
	if (fchmod(dst_fd, src_mode) == -1) {
		perror("reverse_and_copy_file:\n enable to set correct permissions\n");
	}

	// procces things from the name of the function
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;
	off_t position = file_size;
	while (position > 0) {
		size_t to_read = (position >= sizeof(buffer)) ? sizeof(buffer) : position;
		position -= to_read;

		// чтение
		if (lseek(src_fd, position, SEEK_SET) == -1) {
			perror("reverse_and_copy_file:\n lseek\n");
			break;
		}
		bytes_read = read(src_fd, buffer, to_read);
		if (bytes_read <= 0) {
			if (bytes_read == 0) break;
			perror("reverse_and_copy_file:\n read\n");
			break;
		}

		// поворот 
		for (int i = 0; i < bytes_read / 2; i++) {
			char tmp = buffer[i];
			buffer[i] = buffer[bytes_read - 1 - i];
			buffer[bytes_read -1 - i] = tmp;
		}

		// запись
		if (write(dst_fd, buffer, bytes_read) != bytes_read) {
                        perror("reverse_and_copy_file:\n write\n");
			break;
		}
	}

	// finalization
	close(src_fd);
	close(dst_fd);
	return 0;
}

