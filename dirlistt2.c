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


// Home directory
char *home_dir = "/home/song59/Desktop/asp";

// Number of subdirectories for "dirlist -t" feature
static int num_dirs_t = 0;

/* functions for "dirlist -t" */

// Define the struct type DirInfo
typedef struct {
    char name[PATH_MAX];
    time_t created_time;
} DirInfo;

// Point to the first element of the directory struct array
static DirInfo *dir_list = NULL;

// Compare the creation time of directories a and b
// Will be called by qsort() function
int compare_dirinfo(const void *a, const void *b) {
    return ((DirInfo *)a)->created_time - ((DirInfo *)b)->created_time;
}

// Callback function of nftw() to process each directory in the traverse
static int process_dirs_t(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {

    // Check if the current node is a directory and not the root directory
    if (typeflag == FTW_D && strcmp(fpath, home_dir) != 0) {

        // increase the number of directories by 1
        num_dirs_t++;

        // Add more memory in the array
        dir_list = realloc(dir_list, num_dirs_t * sizeof(DirInfo));
        if (!dir_list) {
            fprintf(stderr, "Memory allocation error.\n");
            exit(EXIT_FAILURE);
        }

        // store the information of curent directory in the struct array
        strcpy(dir_list[num_dirs_t - 1].name, fpath + ftwbuf->base);
        dir_list[num_dirs_t - 1].created_time = sb->st_ctime;
    }
    return 0;
}

// Handle "dirlist -t" command
int handle_dirlist_time(int conn) {

    if (!home_dir) {
        fprintf(stderr, "Could not get HOME environment variable.\n");
        exit(EXIT_FAILURE);
    }

    // Traverse the home directory tree
    int flags = FTW_PHYS; // Use physical file type, do not follow symbolic links
    if (nftw(home_dir, process_dirs_t, 10, flags) == -1) {
        perror("nftw");
        exit(EXIT_FAILURE);
    }

    // Sort the directories by creation time
    qsort(dir_list, num_dirs_t, sizeof(DirInfo), compare_dirinfo);

    // Send the number of subdirectories to client
    char message[200];
    sprintf(message, "There are %d subdirectories:\n", num_dirs_t);
    send(conn, message, strlen(message), 0);

    // Send the list of directories to the client
    for (int i = 0; i < num_dirs_t; i++) {
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
        send(conn, dir_list[i].name, strlen(dir_list[i].name), 0);

        // Clear message buffer
        memset(dir_list[i].name, 0, sizeof(dir_list[i].name));

        free(dir_list[i].name);
        free(dir_list[i].created_time);
    }

    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    // Clear message buffer
    memset(message, 0, 201);

    free(dir_list);
    return EXIT_SUCCESS;
}