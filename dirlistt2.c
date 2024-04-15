#define _XOPEN_SOURCE 500 // Required for nftw
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <time.h>
#include <sys/wait.h>

typedef struct {
    char name[PATH_MAX];
    time_t created_time;
} DirInfo;

static int num_dirs = 0;
static DirInfo *dir_list = NULL;
char *home_dir = "/Users/nanasmacbookprowithtouchbar/folder1";

int compare_dirinfo(const void *a, const void *b) {
    return ((DirInfo *)a)->created_time - ((DirInfo *)b)->created_time;
}

static int process_directory(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_D && strcmp(fpath, home_dir) != 0) {

        num_dirs++;
        dir_list = realloc(dir_list, num_dirs * sizeof(DirInfo));
        if (!dir_list) {
            fprintf(stderr, "Memory allocation error.\n");
            exit(EXIT_FAILURE);
        }

        strcpy(dir_list[num_dirs - 1].name, fpath + ftwbuf->base);
        dir_list[num_dirs - 1].created_time = sb->st_ctime;
    }
    return 0;
}

int main() {
    // char *home_dir = getenv("HOME");

    if (!home_dir) {
        fprintf(stderr, "Could not get HOME environment variable.\n");
        exit(EXIT_FAILURE);
    }

    int flags = FTW_PHYS; // Use physical file type, do not follow symbolic links
    if (nftw(home_dir, process_directory, 10, flags) == -1) {
        perror("nftw");
        exit(EXIT_FAILURE);
    }

    // Sort the directories by creation time
    qsort(dir_list, num_dirs, sizeof(DirInfo), compare_dirinfo);

    // Print the sorted list
    for (int i = 0; i < num_dirs; i++) {
        printf("%s\n", dir_list[i].name);
    }

    // Free allocated memory
    free(dir_list);

    return 0;
}
