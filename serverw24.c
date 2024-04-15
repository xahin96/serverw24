#define _XOPEN_SOURCE 500 // Required for nftw

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h> // For fork
#include <string.h> // For memset
#include <time.h>
#include <fcntl.h>
#include <ftw.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <libgen.h>


#define BUFFER_SIZE 32768
#define PORT_SERVER 9055
#define PORT_MIRROR1 9051
#define PORT_MIRROR2 9052


// #define FILENAME "/home/song59/Desktop/asp/temp.tar.gz"

int total_client = 0;
int server_index = 0;

char *my_strdup(const char *str) {
    size_t len = strlen(str) + 1;  // +1 for the null terminator
    char *dup = (char *)malloc(len);  // Allocate memory
    if (dup != NULL) {
        strcpy(dup, str);  // Copy the string
    }
    return dup;
}

void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    while (nanosleep(&ts, &ts) == -1);
}


// Function to trim trailing whitespace characters from a string
void trim_trailing_whitespace(char *str) {
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
}

// functions for "dirlist -a"
// Get the list of subdirectories under home directory in the alphabetical order
char** getSubdirectories_alpha(int *count) {
    FILE *fp;   // file pointer
    char dir_name[1035];    // name of a directory
    char **subdirs = NULL;  // array of pointers referring to all the subdirectories
    int subdir_count = 0;   // number of subdirectories, initialized as 0

    // Run the shell command and its output will be available for reading from 'fp'
    // This command lists all the directories in the user's home directory
    // strips their full path with only directory name left
    // sorts them in a case-insensitive manner
    fp = popen("find ~/Desktop/asp -type d -printf '%P\n' | xargs -n 1 basename | sort -f", "r");
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

// handle "dirlist -a" command received from client
int handle_dirlist_alpha(int conn) {
    int num_subdir;
    char **subdirs = getSubdirectories_alpha(&num_subdir);
    if (subdirs == NULL) {
        fprintf(stderr, "Error: Failed to get subdirectories.\n");
        return EXIT_FAILURE;
    }

    char message[200];
    sprintf(message, "There are %d subdirectories:\n", num_subdir);
    send(conn, message, strlen(message), 0);

    for (int i = 0; i < num_subdir; i++) {
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
        send(conn, subdirs[i], strlen(subdirs[i]), 0);

        // Clear message buffer
        memset(subdirs[i], 0, strlen(subdirs[i]));
        free(subdirs[i]);
    }

    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    free(subdirs);
    return EXIT_SUCCESS;
}

// functions for "dirlist -t"
// Define the struct type DirInfo
typedef struct {
    char name[PATH_MAX];
    time_t created_time;
} DirInfo;

static int num_dirs = 0;    // Number of subdirectories
static DirInfo *dir_list = NULL;    // Point to the first element of the directory struct array
char *home_dir = "/home/song59/Desktop/asp";    // home directory

// Compare two elements in the array
// Will be called by qsort() function
int compare_dirinfo(const void *a, const void *b) {
    return ((DirInfo *)a)->created_time - ((DirInfo *)b)->created_time;
}

// Callback function of nftw() to process each directory in traverse
static int process_directory(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {

    // Check if the current node is a directory and not the root directory
    if (typeflag == FTW_D && strcmp(fpath, home_dir) != 0) {

        // increase the number of directories by 1
        num_dirs++;

        // Add more memory in the array
        dir_list = realloc(dir_list, num_dirs * sizeof(DirInfo));
        if (!dir_list) {
            fprintf(stderr, "Memory allocation error.\n");
            exit(EXIT_FAILURE);
        }

        // Set the curent file
        strcpy(dir_list[num_dirs - 1].name, fpath + ftwbuf->base);
        dir_list[num_dirs - 1].created_time = sb->st_ctime;
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
    if (nftw(home_dir, process_directory, 10, flags) == -1) {
        perror("nftw");
        exit(EXIT_FAILURE);
    }

    // Sort the directories by creation time
    qsort(dir_list, num_dirs, sizeof(DirInfo), compare_dirinfo);

    char message[200];
    sprintf(message, "There are %d subdirectories:\n", num_dirs);
    send(conn, message, strlen(message), 0);

    // Send directories to the client one by one
    for (int i = 0; i < num_dirs; i++) {
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
        send(conn, dir_list[i].name, strlen(dir_list[i].name), 0);

        // Clear message buffer
        memset(dir_list[i].name, 0, sizeof(dir_list[i].name));
    }
    memset(message, 0, sizeof(message)); // Clear message buffer
    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    memset(message, 0, sizeof(message));

    free(dir_list);
    return EXIT_SUCCESS;
}

// store the filename in the w24fn command received from client
char *filename;
// store the information to be send to the client
char name_message[20];
char size_message[20];
char date_message[50];
char permission_message[26];

// nftw() callback function to look for the first occurrence of the input file
int checkFirst(const char *filepath,
               const struct stat *sb,
               int typeflag,
               struct FTW *ftwbuf) {

    // Check if the input file existing in the traversed path
    if (typeflag == FTW_F && strstr(filepath, filename) != NULL) {

        // Format the strings and copy them into variables
        sprintf(name_message, "\nFile: %s\n", filename);
        sprintf(size_message, "Size: %lld bytes\n", sb->st_size);
        sprintf(date_message, "Date of Creation: %s", ctime(&sb->st_ctime));
        sprintf(permission_message,
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
    } else {
        return 0;
    }
}

// Split each command of userInput into argv
char **split_command (char *clientCommands, int *argc) {

    // Copy the command into another pointer
    char *command_copy = my_strdup(clientCommands);

    // Reset the number of arguments as 0
    *argc = 0;
    char **argv = malloc(sizeof(char *) * 6);

    // Get the first token from userInput, which is the name of the command
    char *token = strtok (command_copy, " ");

    while ( token != NULL ) {

        // Duplicate the token to argv[]
        argv[*argc] = my_strdup(token);
        (*argc)++;

        // Split userInput into arguments based on space " "
        token = strtok(NULL, " ");
    }

    // Free the memory allocated for command_copy
    free(command_copy);

    return argv;
}

int handle_w24fn_filename(int conn, char *message) {

    int argc;
    char **argv = split_command(message, &argc);

    filename = argv[1];

    const char *home_dir = getenv("HOME");
    // const char *home_dir = "/home/song59/Desktop/asp/shellscript";   // for testing

    // Traverse all files in the home directory, the second argument
    int searchResult = nftw(home_dir, checkFirst, 20, FTW_NS);

    // nftw() returns 1 if the callback function checkFirst() returns 1
    // Which means the file has been found
    if ( searchResult > 0 ){
        send(conn, name_message, strlen(name_message), 0);
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)

        send(conn, size_message, strlen(size_message), 0);
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)

        send(conn, date_message, strlen(date_message), 0);
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)

        send(conn, permission_message, strlen(permission_message), 0);
    }

    // checkFirst() function returns 0 when the tree is exhausted
    // Which means the traversal was performed but the file was not found
    // In this case, nftw() also returns 0 to searchResult
    else {
        send(conn, "File not found!", strlen("File not found!"), 0);
        printf("\nFile not found!\n\n");
    }

    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    memset(filename, 0, strlen(filename));
    memset(size_message, 0, sizeof(size_message));
    memset(date_message, 0, sizeof(date_message));
    memset(permission_message, 0, sizeof(permission_message));

    return EXIT_SUCCESS;
}

void sendFile(int conn, char *filepath) {
        // Open the file to send
                FILE *file = fopen(filepath, "rb");
                if (!file) {
                    perror("Error opening file, %s");
                    exit(EXIT_FAILURE);
                }

                // Get the file size
                fseek(file, 0, SEEK_END);
                long file_size = ftell(file);
                fseek(file, 0, SEEK_SET);

                printf("file size from server: %ld \n", file_size);

                char file_size_str[20]; // Assuming a maximum of 20 digits for the file size
                sprintf(file_size_str, "%ld", file_size);

                printf("file size from server str: %s\n", file_size_str);

                send(conn, file_size_str, strlen(file_size_str), 0);

                // Send file data to client
                char buffer[BUFFER_SIZE];
                size_t bytes_read;
                int i = 0;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                    printf("bytes found: %d \n", i = i+1);
                    printf("bytes size: %lu \n", sizeof(buffer));
                    sleep_ms(100); // sleep for 1000 milliseconds (1 second)
                    if (send(conn, buffer, bytes_read, 0) != bytes_read) {
                        perror("send");
                        exit(EXIT_FAILURE);
                    }
                }

                fclose(file);
                printf("File sent successfully.\n");
}

int size1, size2;               // store the arguments input by client
int errorFLAGfz = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesfz[100000];      // store all the file paths and names for tar command

// Create a formatted string that represents the file paths and names for archiving
// The shell command format for filepath: -C "filepath1" filename1.extension -C "filepath2" filename2.extension
// Using this format with "-C" is for only archiving the file instead of including all their directories
int combineFileNamefz ( const char *filepath, const char *filename ) {

    // Extract the directory path of the current filepath
    char *file_dir = dirname(filepath);
    printf("path: %s\n", filepath);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNamesfz, "%s -C \"%s\" \"%s\"", allFileNamesfz, file_dir, filename);

    // Return 1 to indicate successful completion
    return 1;
}

int checkSize ( const char *filepath,
                  const struct stat *sb,
                  int typeflag,
                  struct FTW *ftwbuf) {

    printf("path: %s\n", filepath);
    char file_path[PATH_MAX];
    strcpy(file_path, filepath);
    // Check if the size of a file is as request
    if (typeflag == FTW_F && sb->st_size >= size1 && sb->st_size <= size2 ) {

        // Check if the file is existing in allFileNames
        // If not, add its path and name into the allFileName
        if ( strstr(allFileNamesfz, file_path) == NULL ) {
            int a = combineFileNamefz (file_path, file_path + ftwbuf->base );
        }

        // Set this variable as 0 to indicate that searching is successful
        errorFLAGfz = 0;

        // return 0 to make the nftw() function continue the traverse
        return 0;
    }

    // If not existing, it returns 0 to continue the traverse
    else
        return 0;
}

// Handle "w24fz" command
void handle_w24fz_size(int conn, char *message) {

    int argc;
    char **argv = split_command(message, &argc);

    // size1 <= size2
    // size1 >= 0 and size2 >= 0
    size1 = atoi(argv[1]);
    size2 = atoi(argv[2]);

    // char *home_dir = getenv("HOME");
    // Change the home directory later
    char *home_dir = "/home/song59/Desktop/asp";

    // initialize the string
//    *allFileNamesfz = NULL;

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkSize, 20, FTW_PHYS);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAGfz == 0 ) {

            // Create the path of the TAR archive named temp.tar.gz in home directory
            char tar_filepath[PATH_MAX];
            sprintf(tar_filepath, "%s/temp.tar.gz", home_dir);

            // Construct the shell command to compress the files into a TAR archive using "tar -czf"
            char command[100000];   // a string to store the command
            int error = 0;
            sprintf (command, "tar -czf %s%s", tar_filepath, allFileNamesfz);

            printf("command = %s\n", command);
            // Execute the command using system()
            error = system(command);

            // If the TAR archive was created successfully, print successful message
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("\nTAR file created successful! The path is: \n%s\n\n", tar_filepath);
                sendFile(conn, tar_filepath);                
            }

            // Otherwise print a failure message
            else {
                printf("\nTAR file created unsuccessful!\n\n");
                send(conn, "\nTAR file created unsuccessful!\n\n", strlen("\nTAR file created unsuccessful!\n\n"), 0);
            }
        }

        // The value of errorFLAG will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGfz == -1 ) {
            printf("\nNo file found\n\n");
            send(conn, "\nNo file found\n\n", strlen("\nNo file found\n\n"), 0);
        }
    }

    // nftw() returns -1 to searchResult when it detects an error and has not performed the traversal
    else if (searchResult == -1) {
        printf("\nError Searching\n\n");
    }

}

char *extensions[3];
int num_ext;
int errorFLAGft = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesft[100000];      // store all the file paths and names for tar command

// Compare each extension with a filename
// Return 1, if one of the extensions matched
// Return 0, if no extension matched
int compare_extension (char *filename, char *extensions[]) {
    for (int i = 0; i < num_ext; i++) {
        if (strstr(filename, extensions[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

int combineFileNameft ( const char *filepath, const char *filename ) {

    // Extract the directory path of the current filepath
    char *file_dir = dirname (filepath);
    printf("path: %s\n", file_dir);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNamesft, "%s -C \"%s\" \"%s\"", allFileNamesft, file_dir, filename);

    // Return 1 to indicate successful completion
    return 1;
}

// Check each directory in the directory tree
int checkExt ( const char *filepath,
                  const struct stat *sb,
                  int typeflag,
                  struct FTW *ftwbuf) {

    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    // Check if the size of a file is as request
    if (typeflag == FTW_F && compare_extension(file_path, extensions) == 1 ) {
        printf("%s\n", file_path + ftwbuf->base);

        // Check if the file is existing in allFileNames
        // If not, add its path and name into the allFileName
        if ( strstr(allFileNamesft, file_path) == NULL ) {
            int a = combineFileNameft (file_path, file_path + ftwbuf->base );
        }

        // Set this variable as 0 to indicate that searching is successful
        errorFLAGft = 0;

        // return 0 to make the nftw() function continue the traverse
        return 0;
    }

    // If not existing, it returns 0 to continue the traverse
    else
        return 0;
}

// Handle "w24ft" command
void handle_w24ft_ext(int conn, char *message) {

    int argc;
    char **argv = split_command(message, &argc);

    // Number of extension
    num_ext = argc - 1;

    for ( int i = 0; i < num_ext; i++ ) {
        // Allocate memory to each pointer
        extensions[i] = malloc(sizeof(char *));
        // extension is "." + argument
        sprintf(extensions[i], ".%s", argv[i+1]);
        printf("extensions[ %d ] = %s\n", i, extensions[i]);
    }


    // char *home_dir = getenv("HOME");
    // Change the home directory later
    char *home_dir = "/home/song59/Desktop/asp";

    // initialize the string
//    *allFileNamesft = NULL;

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkExt, 20, FTW_PHYS);
    printf("%d\n", searchResult);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAGft == 0 ) {

            // Create the path of the TAR archive named temp.tar.gz in home directory
            char tar_filepath[PATH_MAX];
            sprintf(tar_filepath, "%s/temp.tar.gz", home_dir);

            // Construct the shell command to compress the files into a TAR archive using "tar -czf"
            char command[100000];   // a string to store the command
            int error = 0;
            sprintf (command, "tar -czf %s%s", tar_filepath, allFileNamesft);
            printf("command = %s\n", command);

            // Execute the command using system()
            error = system(command);

            // If the TAR archive was created successfully, print successful message
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("\nTAR file created successful! The path is: \n%s\n\n", tar_filepath);
                sendFile(conn, tar_filepath);
            }

            // Otherwise print a failure message
            else {
                printf("\nTAR file created unsuccessful!\n\n");
                send(conn, "\nTAR file created unsuccessful!\n\n", strlen("\nTAR file created unsuccessful!\n\n"), 0);
            }

        }

        // The value of errorFLAG will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGft == -1 ) {
            printf("\nNo file found\n\n");
            send(conn, "\nNo file found\n\n", strlen("\nNo file found\n\n"), 0);
        }
    }

    // nftw() returns -1 to searchResult when it detects an error and has not performed the traversal
    else if (searchResult == -1) {
        printf("\nError Searching\n\n");
        send(conn, "\nError Searching\n\n", strlen("\nError Searching\n\n"), 0);
    }
}

char *start_date;
int errorFLAGfda = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesfda[100000];      // store all the file paths and names for tar command


// Create a formatted string that represents the file paths and names for archiving
// The shell command format for filepath: -C "filepath1" filename1.extension -C "filepath2" filename2.extension
// Using this format with "-C" is for only archiving the file instead of including all their directories
int combineFileNamefda ( const char *filepath, const char *filename ) {

    // Extract the directory path of the current filepath
    char *file_dir = dirname (filepath);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNamesfda, "%s -C \"%s\" \"%s\"", allFileNamesfda, file_dir, filename);
    // printf("allFileNamesfda = %s\n", allFileNamesfda);

    // Return 1 to indicate successful completion
    return 1;
}

int checkDateAfter ( const char *filepath,
                  const struct stat *sb,
                  int typeflag,
                  struct FTW *ftwbuf) {
    
    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    char ctime[15];
    strftime(ctime, sizeof(ctime), "%Y-%m-%d", localtime(&sb->st_ctime));

    // Check if the creation date of a file is larger than the start date input from client
    if (typeflag == FTW_F && strcmp(ctime, start_date) >= 0 ) {
        
        printf("file_path: %s\n", file_path);

        // Check if the file is existing in allFileNamesfda
        // If not, add its path and name into the allFileName
        if ( strstr(allFileNamesfda, file_path) == NULL ) {
            int a = combineFileNamefda (file_path, file_path + ftwbuf->base );
        }

        // Set this variable as 0 to indicate that searching is successful
        errorFLAGfda = 0;

        // return 0 to make the nftw() function continue the traverse
        return 0;
    }

    // If not existing, it returns 0 to continue the traverse
    else
        return 0;
}

// Create a TAR file that contains all the files founded in home directory, which is created after an start_date
void handle_w24fda_after(int conn, char *message) {

    int argc;
    char **argv = split_command(message, &argc);

    start_date = argv[1];

    // char *home_dir = getenv("HOME");
    // Change the home directory later
    char *home_dir = "/home/song59/Desktop/asp/shellscript";

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkDateAfter, 20, FTW_PHYS);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAGfda == 0 ) {
            // printf("Search successful! All your requested files are showed above!\n\n");

            // Create the path of the TAR archive named temp.tar.gz in home directory
            char tar_filepath[PATH_MAX];
            sprintf(tar_filepath, "%s/temp.tar.gz", home_dir);

            // Construct the shell command to compress the files into a TAR archive using "tar -czf"
            char command[100000];   // a string to store the command
            int error = 0;
            sprintf (command, "tar -czf %s%s", tar_filepath, allFileNamesfda);

            // Execute the command using system()
            error = system(command);

            // If the TAR archive was created successfully, print successful message
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("\nTAR file created successful! The path is: \n%s\n\n", tar_filepath);
                sendFile(conn, tar_filepath);
            }

            // Otherwise print a failure message
            else {
                printf("\nTAR file created unsuccessful!\n\n");
                send(conn, "\nTAR file created unsuccessful!\n\n", strlen("\nTAR file created unsuccessful!\n\n"), 0);            
            }
        }

        // The value of errorFLAGfda will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGfda == -1 ) {
            printf("\nNo file found\n\n");
            send(conn, "\nNo file found\n\n", strlen("\nNo file found\n\n"), 0);
        }
    }

    // nftw() returns -1 to searchResult when it detects an error and has not performed the traversal
    else if (searchResult == -1){
        printf("\nError Searching\n\n");
        send(conn, "\nError Searching\n\n", strlen("\nError Searching\n\n"), 0);
    }
}

char *end_date;
int errorFLAGfdb = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesfdb[100000];      // store all the file paths and names for tar command

// Create a formatted string that represents the file paths and names for archiving
// The shell command format for filepath: -C "filepath1" filename1.extension -C "filepath2" filename2.extension
// Using this format with "-C" is for only archiving the file instead of including all their directories
int combineFileNamefdb ( const char *filepath, const char *filename ) {
    char quoted_path[PATH_MAX + 3];     // Store quoted directory

    // Extract the directory path of the current filepath
    char *file_dir = dirname (filepath);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNamesfdb, "%s -C \"%s\" \"%s\"", allFileNamesfdb, file_dir, filename);

    // Return 1 to indicate successful completion
    return 1;
}

int checkDateBefore ( const char *filepath,
                  const struct stat *sb,
                  int typeflag,
                  struct FTW *ftwbuf) {

    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    char ctime[15];
    strftime(ctime, sizeof(ctime), "%Y-%m-%d", localtime(&sb->st_ctime));
    
    printf("ctime = %s\n", ctime);
    printf("end date = %s\n", end_date);

    // Check if the creation date of a file is as request
    if (typeflag == FTW_F && strcmp(ctime, end_date) <= 0 ) {
        
        printf("file_path: %s\n ctime: %s\n\n", file_path + ftwbuf->base, ctime);

        // Check if the file is existing in allFileNamesfdb
        // If not, add its path and name into the allFileName
        if ( strstr(allFileNamesfdb, file_path) == NULL ) {
            int a = combineFileNamefdb (file_path, file_path + ftwbuf->base );
        }

        // Set this variable as 0 to indicate that searching is successful
        errorFLAGfdb = 0;

        // return 0 to make the nftw() function continue the traverse
        return 0;
    }

    // If not existing, it returns 0 to continue the traverse
    else
        return 0;
}

// Create a TAR file that contains all the files founded in home directory, which is created before an end_date
void handle_w24fdb_before ( int conn, char *message ) {

    int argc;
    char **argv = split_command(message, &argc);

    end_date = argv[1];

    // char *home_dir = getenv("HOME");
    // Change the home directory later
    char *home_dir = "/home/song59/Desktop/asp/shellscript/test";

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkDateBefore, 20, FTW_PHYS);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAGfdb == 0 ) {
            // printf("Search successful! All your requested files are showed above!\n\n");

            // Create the path of the TAR archive named temp.tar.gz in home directory
            char tar_filepath[PATH_MAX];
            sprintf(tar_filepath, "%s/temp.tar.gz", home_dir);

            // Construct the shell command to compress the files into a TAR archive using "tar -czf"
            char command[100000];   // a string to store the command
            int error = 0;
            sprintf (command, "tar -czf %s%s", tar_filepath, allFileNamesfdb);

            // Execute the command using system()
            error = system(command);

            // If the TAR archive was created successfully, print successful message
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("\nTAR file created successful! The path is: \n%s\n\n", tar_filepath);
                sendFile(conn, tar_filepath);            
            }

            // Otherwise print a failure message
            else {
                printf("\nTAR file created unsuccessful!\n\n");
                send(conn, "\nTAR file created unsuccessful!\n\n", strlen("\nTAR file created unsuccessful!\n\n"), 0);            
            }
        }

        // The value of errorFLAGfdb will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGfdb == -1 ) {
            printf("\nNo file found\n\n");
            send(conn, "\nNo file found\n\n", strlen("\nNo file found\n\n"), 0);
        }
    }

    // nftw() returns -1 to searchResult when it detects an error and has not performed the traversal
    else if (searchResult == -1) {
        printf("\nError Searching\n\n");
        send(conn, "\nError Searching\n\n", strlen("\nError Searching\n\n"), 0);
    }
}


// Function to handle client requests
void crequest(int conn, int server_port) {
    char message[100] = "";
    send(conn, "Server will handle the request", strlen("Server will handle the request"), 0);

    while (1) {
        // Receive message from client
        if (recv(conn, message, sizeof(message), 0) > 0) {
            // Trim the white space after the message
            trim_trailing_whitespace(message);

            printf("Client message: %s\n", message);

            if (strstr(message, "dirlist -a") != NULL) {
                handle_dirlist_alpha(conn);
            }

            if (strstr(message, "dirlist -t") != NULL) {
                handle_dirlist_time(conn);
            }

            if (strstr(message, "w24fn") != NULL) {
                handle_w24fn_filename(conn, message);
            }

            if (strstr(message, "w24fz") != NULL) {
                handle_w24fz_size(conn, message);
            }

            if (strstr(message, "w24ft") != NULL) {
                handle_w24ft_ext(conn, message);
            }

            if (strstr(message, "w24fda") != NULL) {
                handle_w24fda_after(conn, message);
            }

            if (strstr(message, "w24fdb") != NULL) {
                handle_w24fdb_before(conn, message);
            }

            // Check for exit condition
            if (strncmp(message, "quitc", 5) == 0) {
                break;
            }

            // If client is handled by a mirror server, send message to reconnect
            if (server_port != PORT_SERVER) {
                sprintf(message, "CONNECT_TO_PORT:%d", server_port);
                send(conn, message, strlen(message), 0);
            }
        }
    }

    close(conn); // Close the connection socket
}


int main() {
    struct sockaddr_in server;
    int fd_server, fd_client;

    // Initialize server socket structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    // Create server socket
    if ((fd_server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind server socket to address
    server.sin_port = htons(PORT_SERVER);
    if (bind(fd_server, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for client connections
    if (listen(fd_server, 15) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept and handle connections
    // Accept and handle connections
    while ((fd_client = accept(fd_server, (struct sockaddr *)NULL, NULL))) {
        int current_port;

        if (total_client < 3) {
            // Increment total_client count and print
            printf("Client handled by serverw24\n");
            current_port = PORT_SERVER;
        } else if (total_client < 6) {
            printf("Client handled by mirror1\n");
            send(fd_client, "CONNECT_TO_PORT:9051", strlen("CONNECT_TO_PORT:9051"), 0);
            current_port = PORT_MIRROR1;
        } else if (total_client < 9) {
            printf("Client handled by mirror2\n");
            send(fd_client, "CONNECT_TO_PORT:9052", strlen("CONNECT_TO_PORT:9052"), 0);
            current_port = PORT_MIRROR2;
        } else {
            // Cycle through server, mirror1, and mirror2
            if (server_index == 0) {
                printf("Client handled by serverw24\n");
                current_port = PORT_SERVER;
            } else if (server_index == 1) {
                printf("Client handled by mirror1\n");
                send(fd_client, "CONNECT_TO_PORT:9051", strlen("CONNECT_TO_PORT:9051"), 0);
                current_port = PORT_MIRROR1;
            } else {
                printf("Client handled by mirror2\n");
                send(fd_client, "CONNECT_TO_PORT:9052", strlen("CONNECT_TO_PORT:9052"), 0);
                current_port = PORT_MIRROR2;
            }
            server_index = (server_index + 1) % 3;
        }

        // Increment total_client count
        total_client++;

        // Fork a child process to handle client request
        int pid;
        if ((pid = fork()) == 0) {
            // Child process
            close(fd_server); // Close the listening socket in the child process
            crequest(fd_client, current_port); // Handle client requests
            exit(EXIT_SUCCESS);
        } else if (pid == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            close(fd_client); // Close the connection socket in the parent process
        }
    }


    if (fd_client == -1) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}
