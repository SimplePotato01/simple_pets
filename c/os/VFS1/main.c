#include "boilerplate.h"

int process_directory(const char *src_dir){
	if (check_existence(src_dir) != 0) {
		return -1;
	}
	struct stat dir_stat;
	if (stat(src_dir, &dir_stat) != 0){
		perror("process_directory:\n");
		return -1;
	}
	mode_t src_dir_mode = dir_stat.st_mode & 07777;
	
	char *last_slash = strrchr(src_dir, '/');
	const char *src_dir_name = (last_slash != NULL) ? last_slash + 1 : src_dir;
	char reversed_src_dir_name[256];
	reverse_string(src_dir_name, reversed_src_dir_name);
	
	char dst_dir_path[256];
	if (last_slash != NULL) {
		// копия пути до последнего слеша
		size_t path_len = last_slash - src_dir_name + 1;
		strncpy(dst_dir_path, src_dir, path_len);
		dst_dir_path[path_len] = '\0';
		strcat(dst_dir_path, reversed_src_dir_name);
	} else {
		// исходная дир. находится в текущей папке
		strcpy(dst_dir_path, reversed_src_dir_name);
	}
#if DEBUG == 1
    	printf("source directory: %s\n", src_dir);
   	printf("source directory permissions: %o\n", src_dir_mode);
 	printf("sarget directory: %s\n", dst_dir_path);
#endif

	if(create_dir(dst_dir_path, src_dir_mode) == -1){
		perror("create_dir in process_directory:\n");
	}
	DIR *dir = opendir(src_dir);
	if (dir == NULL) {
		perror("process_directory:\n");
                return -1;
	}

	struct dirent *entry;
	int success_count = 0;
	int error_count = 0;
	// cycle of copy
	while ((entry = readdir(dir)) != NULL) {
        	// skipping default . & ..
        	if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            		continue;
        	}

        	// Формируем полный путь к файлу в исходной директории
        	char src_file_path[512];
        	snprintf(src_file_path, sizeof(src_file_path), "%s/%s", src_dir, entry->d_name);

        	// checking is it a regular file
        	if (!is_regular_file(src_file_path)) {
            		printf("here was a non-regular_file: %s\n", entry->d_name);
            		continue;
        	}

        	// Получаем права доступа исходного файла для отображения
        	mode_t file_mode;
        	if (get_file_permissions(src_file_path, &file_mode) == 0) {
            		printf("File %s has permissions: %o\n", entry->d_name, file_mode);
        	}

        	// Переворачиваем имя файла
        	char reversed_filename[256];
        	reverse_string(entry->d_name, reversed_filename);

        	// Формируем полный путь к целевому файлу
        	char dst_file_path[512];
        	snprintf(dst_file_path, sizeof(dst_file_path), "%s/%s", dst_dir_path, reversed_filename);
		#if DEBUG == 1
        		printf("Copying: %s -> %s\n", src_file_path, dst_file_path);
		#endif

        	// Копируем файл с переворотом содержимого и сохранением прав
        	if (reverse_and_copy_file(src_file_path, dst_file_path) == 0) {
            		success_count++;
        	} else {
            		error_count++;
        	}
    	}
    	closedir(dir);
	return 0;
}

int main(int argc, char *argv[]){
	if (argc < 2) {
		printf("you have to write directory name, error!\n");
		return -1;
	}
	return process_directory(argv[1]);
}
