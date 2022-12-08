#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

void handle_error(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// copy file
void copy_file(const char *source, const char *destination) {
    const u_int32_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    u_int32_t bytes_read;

    FILE *input_file = fopen(source, "r");
    if (input_file == NULL) {
        handle_error("open source file fail");
    }
    //auto create file
    FILE *output_file = fopen(destination, "w");
    if (output_file == NULL) {
        handle_error("open source file fail");
    }
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, input_file)) > 0) {
        fwrite(buffer, 1, bytes_read, output_file);
    }
    // close file
    fclose(input_file);
    fclose(output_file);
}

void copy_directory(const char *source, const char *destination) {

    //mkdir
    if (mkdir(destination, 0777) != 0) {
        char tmp[256];
        sprintf(tmp, "create destination directory %s fail", destination);
        handle_error(tmp);
    }

    DIR *source_dir = opendir(source);
    if (source_dir == NULL) {
        handle_error("Error opening source directory");
    }

    struct dirent *entry;
    while ((entry = readdir(source_dir)) != NULL) {
        if(entry->d_name[0]=='.'){
            continue;
        }
        char *source_path = malloc(strlen(source) + strlen(entry->d_name) + 2);
        sprintf(source_path, "%s/%s", source, entry->d_name);
        char *destination_path = malloc(strlen(destination) + strlen(entry->d_name) + 2);
        sprintf(destination_path, "%s/%s", destination, entry->d_name);

        struct stat stat_buf;
        if (stat(source_path, &stat_buf) != 0) {
            handle_error(" get file type fail");
        }
        if (S_ISREG(stat_buf.st_mode)) {
            copy_file(source_path, destination_path);
        } else if (S_ISDIR(stat_buf.st_mode)) {
            copy_directory(source_path, destination_path);
        }

        free(source_path);
        free(destination_path);
    }

    closedir(source_dir);
}

int main_cp_R(int argc, char *argv[]) {

    if (argc != 3) {
        printf("USAGE: CMD [src] [dst] \n");
        exit(EXIT_FAILURE);
    }
    //must detect infinity copy

    const char *source = argv[1];
    const char *destination = argv[2];

    struct stat stat_buf;
    if (stat(source, &stat_buf) != 0) {
        handle_error("get src file type fail");
    }

    if (S_ISREG(stat_buf.st_mode)) {
        //copy file
        copy_file(source, destination);
    } else if (S_ISDIR(stat_buf.st_mode)) {
        //copy dir


        copy_directory(source, destination);
    }

    return 0;
}

