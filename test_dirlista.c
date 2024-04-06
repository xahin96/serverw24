#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** getSubdirectories(int *count) {
    FILE *fp;
    char path[1035];
    char **subdirs = NULL;
    int subdir_count = 0;

    // Open the command for reading
    fp = popen("ls -1d $HOME/*/ | xargs -n 1 basename | sort -f", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run command\n");
        return NULL;
    }

    // Read the output line by line
    while (fgets(path, sizeof(path)-1, fp) != NULL) {
        // Remove newline character
        path[strcspn(path, "\n")] = 0;

        // Allocate memory for the new subdir name
        subdirs = realloc(subdirs, (subdir_count + 1) * sizeof(char *));
        subdirs[subdir_count] = malloc(strlen(path) + 1);
        strcpy(subdirs[subdir_count], path);
        subdir_count++;
    }

    // Close the pipe and get the return code
    int ret = pclose(fp);
    if (ret == -1) {
        fprintf(stderr, "Error while closing pipe\n");
        return NULL;
    }

    *count = subdir_count;
    return subdirs;
}

int main() {
    int count;
    char **subdirs = getSubdirectories(&count);
    if (subdirs == NULL) {
        fprintf(stderr, "Error: Failed to get subdirectories.\n");
        return EXIT_FAILURE;
    }

    printf("Subdirectories in %s:\n", getenv("HOME"));
    for (int i = 0; i < count; i++) {
        printf("%s\n", subdirs[i]);
        free(subdirs[i]);
    }

    free(subdirs);

    return EXIT_SUCCESS;
}
