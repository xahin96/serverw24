#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <time.h>

#define MAX_NUM_COMMANDS 6     // up to 5 operations be supported

char *filename;  // store the name of the input file
char size_message[13];
char date_message[50];
char permission_message[26];

char** getSubdirectories_alpha(int *count) {
    FILE *fp;   // file pointer
    char dir_name[1035];    // name of a directory
    char **subdirs = NULL;  // array of pointers referring to all the subdirectories
    int subdir_count = 0;   // number of subdirectories, initialized as 0

    // Run the shell command and its output will be available for reading from 'fp'
    // This command lists all the directories in the user's home directory
    // strips their full path with only directory name left
    // sorts them in a case-insensitive manner
    fp = popen("ls -1d $HOME/*/ | xargs -n 1 basename | sort -f", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run command\n");
        return NULL;
    }

    // Read the output from 'fp' line by line
    while (fgets(dir_name, sizeof(dir_name)-1, fp) != NULL) {
        // Remove newline character
        dir_name[strcspn(dir_name, "\n")] = 0;

        // Change the size of the array of subdirectories
        subdirs = realloc(subdirs, (subdir_count + 1) * sizeof(char *));
        // Allocate memory for a new element of the subdirectories array
        subdirs[subdir_count] = malloc(strlen(dir_name) + 1);

        // Copy new subdirectory name read from 'fp' into the current array element
        strcpy(subdirs[subdir_count], dir_name);
        subdir_count++;
    }

    // Close the pipe and get the return code
    int ret = pclose(fp);
    if (ret == -1) {
        fprintf(stderr, "Error while closing pipe !!!\n");
        return NULL;
    }

    *count = subdir_count;
    return subdirs;
}

int dirlista(int argc, char *argv[]) {
    int count;
    char **subdirs = getSubdirectories_alpha(&count);
    if (subdirs == NULL) {
        fprintf(stderr, "Error: Failed to get subdirectories.\n");
        return EXIT_FAILURE;
    }

    printf("There are totally %d subdirectories in %s:\n", count, getenv("HOME"));
    for (int i = 0; i < count; i++) {
        printf("%s\n", subdirs[i]);
        free(subdirs[i]);
    }

    free(subdirs);

    return EXIT_SUCCESS;
}


char** getSubdirectories_time(int *count) {
    FILE *fp;   // file pointer
    char dir_name[1035];    // name of a directory
    char **subdirs = NULL;  // array of pointers referring to all the subdirectories
    int subdir_count = 0;   // number of subdirectories, initialized as 0

    // Run the shell command and its output will be available for reading from 'fp'
    // This command lists all the directories in the user's home directory
    // strips their full path with only directory name left
    // sorts them in a case-insensitive manner
    fp = popen("ls -1 -d -tr --time=birth $HOME/*/ | xargs -n 1 basename", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run command!\n");
        return NULL;
    }

    // Read the output from 'fp' line by line
    while (fgets(dir_name, sizeof(dir_name)-1, fp) != NULL) {
        // Remove newline character
        dir_name[strcspn(dir_name, "\n")] = 0;

        // Change the size of the array of subdirectories
        subdirs = realloc(subdirs, (subdir_count + 1) * sizeof(char *));
        // Allocate memory for a new element of the subdirectories array
        subdirs[subdir_count] = malloc(strlen(dir_name) + 1);

        // Copy new subdirectory name read from 'fp' into the current array element
        strcpy(subdirs[subdir_count], dir_name);
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

int dirlistt(int argc, char *argv[]) {
    int count;
    char **subdirs = getSubdirectories_time(&count);
    if (subdirs == NULL) {
        fprintf(stderr, "Error: Failed to get subdirectories.\n");
        return EXIT_FAILURE;
    }

    printf("There are totally %d subdirectories in %s:\n", count, getenv("HOME"));
    for (int i = 0; i < count; i++) {
        printf("%s\n", subdirs[i]);
        free(subdirs[i]);
    }

    free(subdirs);

    return EXIT_SUCCESS;
}

// nftw() callback function to look for the first occurrence of the input file
int checkFirst ( const char *filepath,
                 const struct stat *sb,
                 int typeflag,
                 struct FTW *ftwbuf ) {
    
    // Check if the input file existing in the traversed path
    if (typeflag == FTW_F && strstr(filepath, filename) != NULL) {

        // printf("File: %s\n", filename);
        // printf("Size: %ld bytes\n", sb->st_size);
        snprintf(size_message, sizeof(size_message), "Size: %ld bytes\n", sb->st_size);
        // printf("Date of Creation: %s\n", ctime(&sb->st_ctime));
        snprintf(date_message, sizeof(date_message), "Date of Creation: %s\n", ctime(&sb->st_ctime));
        snprintf(permission_message, sizeof(permission_message),
                "Permissions: %c%c%c%c%c%c%c%c%c%c\n",
                (S_ISDIR(sb->st_mode)) ? 'd' : '-',
               (sb->st_mode & S_IRUSR) ? 'r' : '-',
               (sb->st_mode & S_IWUSR) ? 'w' : '-',
               (sb->st_mode & S_IXUSR) ? 'x' : '-',
               (sb->st_mode & S_IRGRP) ? 'r' : '-',
               (sb->st_mode & S_IWGRP) ? 'w' : '-',
               (sb->st_mode & S_IXGRP) ? 'x' : '-',
               (sb->st_mode & S_IROTH) ? 'r' : '-',
               (sb->st_mode & S_IWOTH) ? 'w' : '-',
               (sb->st_mode & S_IXOTH) ? 'x' : '-');

        // return 1 to make the nftw() function stop the traverse after the first occurrence
        return 1;
    }
    else
        return 0;
}

int w24fn(int argc, char *argv[]) {

    filename = argv[1];
    const char *home_dir = getenv("HOME");

    // Traverse all files in the home directory, the second argument
    int searchResult = nftw(home_dir, checkFirst, 20, FTW_NS);

    // nftw() returns 1 if the callback function checkFirst() returns 1
    // Which means the file has been found
    if ( searchResult > 0 ){
        printf("File: %s\n", filename);
        printf("%s\n", size_message);
        printf("%s", date_message);
        printf("%s\n", permission_message);
    }

    // checkFirst() function returns 0 when the tree is exhausted
    // Which means the traversal was performed but the file was not found
    // In this case, nftw() also returns 0 to searchResult
    else {
        printf("\nFile not found!\n\n");
    }

    return 0;
}

// Split each command of userInput into argv
char **split_command (char *clientCommands, int *argc) {

    // Copy the command into another pointer
    char *command_copy = strdup(clientCommands);

    // Reset the number of arguments as 0
    *argc = 0;
    char **argv = malloc(sizeof(char *) * 6);

    // Get the first token from userInput, which is the name of the command
    char *token = strtok (command_copy, " ");

    while ( token != NULL ) {

        // Duplicate the token to argv[]
        argv[*argc] = strdup (token);
        (*argc)++;

        // Split userInput into arguments based on space " "
        token = strtok(NULL, " ");
    }

    // Free the memory allocated for command_copy
    free(command_copy);
    return argv;
}

// Check the userInput string
void check_commands(char *userInput) {

    int argc;
    char **argv = split_command(userInput, &argc);

    // dirlist
    if ( strcmp( argv[0], "dirlist" ) == 0 ) {
        if (strcmp(argv[1], "-a" ) == 0 ) {
           dirlista(argc, argv);
        }
        else if ( strcmp( argv[1], "-t" ) == 0 ) {
           dirlistt(argc, argv);
        }

    }

    // w24fn
    if ( strcmp( argv[0], "w24fn" ) == 0  ) {
        w24fn(argc, argv);
    }

    // w24fz
    if ( strcmp( argv[0], "w24fz" ) == 0 ) {
        printf("argc = %d\n", argc);
        for (int i = 0; i < argc; i++) {
            printf("argv[ %d ] = %s\n", i, argv[i]);
        }
        // w24fz(argc, argv);
    }

    // w24ft
    if ( strcmp( argv[0], "w24ft" ) == 0 ) {
        // w24ft(argc, argv);
    }

    // w24fdb
    if ( strcmp( argv[0], "w24fdb" ) == 0 ) {
        // w24fdb(argc, argv);
    }

    // w24fda
    if ( strcmp( argv[0], "w24fa" ) == 0 ) {
        // w24fda(argc, argv);
    }

    // quitc
    if ( strcmp( argv[0], "quitc" ) == 0 ) {
        // quitc(argc, argv);
    }

    free(argv);
}

int main() {

    /* Append the current working directory to PATH so that the executables in this directory
    can be run without specifying their path */
//    append_cwd();

    char userInput[10000];  // storing the string input by user

    // Infinite loop
    while(1) {

        fflush(stdout);
        fflush(stdin);

        // Waiting for user's input
        printf("\nclientw24$ ");

        // read a string input by users until a newline '\n' is input
        /* the space before '%' make 'scanf' to ignore any leading whitespace
            including spaces, tabs, and newlines before reading */
        scanf(" %[^\n]", userInput);

        check_commands(userInput);

//        free(userInput);
//        userInput[0] = '\0';
    }

    return 0;
}