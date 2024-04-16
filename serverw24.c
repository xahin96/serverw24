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
#define PORT_SERVER 9059
#define PORT_MIRROR1 9051
#define PORT_MIRROR2 9052

// Home directory
char *home_dir = "/home/song59/Desktop/asp";


int total_client = 0;
int server_index = 0;

/* Variables for "w24fn" feature */
char *filename;         // store the filename in the w24fn command received from client
char name_message[20];  // store the information to be send to the client
char size_message[20];
char date_message[50];
char permission_message[26];

/* Variables for "w24fz" feature */
int size1, size2;               // store the arguments input by client
int errorFLAGfz = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesfz[100000];      // store all the file paths and names for tar command

/* Variables for "w24ft" feature */
char *extensions[3];              // store at most 3 extensions
int num_ext;                      // Number of extensions
int errorFLAGft = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesft[100000];      // store all the file paths and names for tar command

/* Variables for "w24fda" feature */
char *start_date;                  // store the start date input from client
int errorFLAGfda = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesfda[100000];      // store all the file paths and names for tar command

/* Variables for "w24fdb" feature */
char *end_date;                    // store the end date input from client
int errorFLAGfdb = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesfdb[100000];      // store all the file paths and names for tar command


// Copy str and return a string referred by dup
char *my_strdup(const char *str) {
    size_t len = strlen(str) + 1;  // +1 for the null terminator
    char *dup = (char *)malloc(len);  // Allocate memory
    if (dup != NULL) {
        strcpy(dup, str);  // Copy the string
    }
    return dup;
}

// Sleep
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

// Split each command of userInput into argv array
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

/* functions for "dirlist -a" */
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

// handle "dirlist -a"
int handle_dirlist_alpha(int conn) {

    // Number of the subdirectories
    int num_subdir;

    // Get the list of subdirectories in the alphabetical order
    char **subdirs = getSubdirectories_alpha(&num_subdir);
    if (subdirs == NULL) {
        fprintf(stderr, "Error: Failed to get subdirectories.\n");
        return EXIT_FAILURE;
    }

    // Send the number of subdirectories to client
    char message[200];
    sprintf(message, "There are %d subdirectories:\n", num_subdir);
    send(conn, message, strlen(message), 0);

    // Send the list of subdirectories to client
    for (int i = 0; i < num_subdir; i++) {
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
        send(conn, subdirs[i], strlen(subdirs[i]), 0);

        // Clear message buffer
        memset(subdirs[i], 0, strlen(subdirs[i]));
        // free the memory of each element in subdirs list
        free(subdirs[i]);
    }

    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    free(subdirs);
    return EXIT_SUCCESS;
}

/* functions for "dirlist -t" */

char** getSubdirectories_time(int *count) {
    FILE *fp;   // file pointer
    char dir_name[1035];    // name of a directory
    char **subdirs = NULL;  // array of pointers referring to all the subdirectories
    int subdir_count = 0;   // number of subdirectories, initialized as 0

    // Run the shell command and its output will be available for reading from 'fp'
    // This command lists all the directories in the order of creation time and strips their full path with only directory name left
    fp = popen("ls -1 -d -tr --time=birth $HOME/*/ | xargs -n 1 basename", "r");
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
        fprintf(stderr, "Error while closing pipe\n");
        return NULL;
    }

    *count = subdir_count;
    return subdirs;
}

// Handle "dirlist -t" command
int handle_dirlist_time(int conn) {

    // Number of the subdirectories
    int num_subdir;

    // Get the list of subdirectories in the alphabetical order
    char **subdirs = getSubdirectories_time(&num_subdir);
    if (subdirs == NULL) {
        fprintf(stderr, "Error: Failed to get subdirectories.\n");
        return EXIT_FAILURE;
    }

    // Send the number of subdirectories to client
    char message[200];
    sprintf(message, "There are %d subdirectories:\n", num_subdir);
    send(conn, message, strlen(message), 0);

    // Send the list of subdirectories to client
    for (int i = 0; i < num_subdir; i++) {
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
        send(conn, subdirs[i], strlen(subdirs[i]), 0);

        // Clear message buffer
        memset(subdirs[i], 0, strlen(subdirs[i]));
        // free the memory of each element in subdirs list
        free(subdirs[i]);
    }

    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    free(subdirs);
    return EXIT_SUCCESS;
}

/* Functions for "w24fn" */

// nftw() callback function to look for the first occurrence of the input file
int checkFirst(const char *filepath,
               const struct stat *sb,
               int typeflag,
               struct FTW *ftwbuf) {

    // printf("filepath-> %s\n", filepath);
    // printf("filename-> %s\n", filename);

    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    // printf("file_path-> %s\n", file_path);
    // printf("------\n%d\n", strstr(file_path, filename));

    // Check if the current path is a file and contains the name input by client
    if (typeflag == FTW_F && strstr(file_path, filename) != NULL) {
        // printf("file_path---> %s\n", file_path);
        // printf("filename---> %s\n", filename);

        // Format the strings and copy them into message variables
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

// Handle "w24fn" command
int handle_w24fn_filename(int conn, char *message) {

    int argc;
    // Split the commands into an array

    char **argv = split_command(message, &argc);

    // The second argument is the filename
    filename = argv[1];

    // Traverse all files in the home directory
    int sr = nftw(home_dir, checkFirst, 20, FTW_NS);
            printf("ok1\n");

    printf("searchResult = %d\n", sr);
            printf("ok2\n");


    // nftw() returns 1 if the callback function checkFirst() returns 1, which means the file has been found
    if ( sr > 0 ){

        // Send the all file information to the client
        send(conn, name_message, strlen(name_message), 0);
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)

        send(conn, size_message, strlen(size_message), 0);
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)

        send(conn, date_message, strlen(date_message), 0);
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)

        send(conn, permission_message, strlen(permission_message), 0);
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
    }
    // checkFirst() function returns 0 when the tree is exhausted, which means the traversal was performed but the file was not found
    // In this case, nftw() also returns 0 to searchResult
    else {
        send(conn, "NON_FILE", strlen("NON_FILE"), 0);
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
        printf("File not found!\n");
    }

    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    // Clear all the buffers
    memset(name_message, 0, sizeof(name_message));
    memset(size_message, 0, sizeof(size_message));
    memset(date_message, 0, sizeof(date_message));
    memset(permission_message, 0, sizeof(permission_message));

    return EXIT_SUCCESS;
}

// Send TAR file from the server to the client
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

    char file_size_str[20]; // Assuming a maximum of 20 digits for the file size
    sprintf(file_size_str, "%ld", file_size);

    send(conn, file_size_str, strlen(file_size_str), 0);

    // Send file data to client
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int i = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
        if (send(conn, buffer, bytes_read, 0) != bytes_read) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }

    fclose(file);
    printf("File sent successfully.\n");
}

/* Functions for "w24fz" feature */

// Create a formatted string that represents the file paths and names for archiving
// The shell command format for filepath: -C "filepath1" filename1.extension -C "filepath2" filename2.extension
// Using this format with "-C" is for only archiving the file instead of including all their directories
int combineFileNamefz ( char *filepath, char *filename ) {

    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    // Extract the directory path of the current filepath
    char *file_dir = dirname(file_path);
    printf("path: %s\n", file_path);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNamesfz, "%s -C \"%s\" \"%s\"", allFileNamesfz, file_dir, filename);

    // Return 1 to indicate successful completion
    return 1;
}

// Check if the size of current filepath is as requested
int checkSize ( const char *filepath,
                const struct stat *sb,
                int typeflag,
                struct FTW *ftwbuf) {

    // Copy the filepath into a new string
    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    // Check if the size of the current file is between the range
    if  (typeflag == FTW_F && sb->st_size >= size1 && sb->st_size <= size2 ) {

        // Check if the file exists in the string allFileNames
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
    else {
        return 0;
    }
}

// Handle "w24fz" command
void handle_w24fz_size(int conn, char *message) {
    errorFLAGfz = -1;

    int argc;   // Number of arguments
    // Split the command into argv array
    char **argv = split_command(message, &argc);

    // Convert the char arguments to integers
    size1 = atoi(argv[1]);
    size2 = atoi(argv[2]);

    // initialize the string
    *allFileNamesfz = '\0';

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

            // Execute the command using system()
            error = system(command);

            // If the TAR archive was created successfully, print and send successful message
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("TAR file created successful! The path is: \n%s\n", tar_filepath);
                // Send the TAR file to the clinet
                sendFile(conn, tar_filepath);
            }

            // Otherwise print and send a failure message
            else {
                send(conn, "Creation failed", strlen("Creation failed"), 0);
                sleep_ms(200); // sleep for 1000 milliseconds (1 second)
                printf("TAR file created unsuccessful!\n");
            }
        }

        // The value of errorFLAGfz will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGfz == -1 ) {
            send(conn, "No file found", strlen("No file found"), 0);
            sleep_ms(200); // sleep for 200 milliseconds
            printf("No file found\n");
        }
    }

    // When it detects an error and has not performed the traversal
    else {
        send(conn, "No file found", strlen("No file found"), 0);
        sleep_ms(200); // sleep for 200 milliseconds
        printf("Error Searching\n");
    }

}

/* Functions for "w24ft" feature */

// Compare each element pointed by *extensions with a filename
// Return 1, if at least one of the extensions matched
// Return 0, if no extension matched
int compare_extension (char *filename, char *extensions[]) {
    for (int i = 0; i < num_ext; i++) {
        if (strstr(filename, extensions[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

// Store filepath and filename in a string with certain format for tar creating command
int combineFileNameft ( char *filepath, char *filename ) {

    // Extract the directory path of the current filepath
    char *file_dir = dirname (filepath);

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

    // Copy the filepath into a new string
    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    // Check if the extension of a file is as request
    if (typeflag == FTW_F && compare_extension(file_path, extensions) == 1 ) {

        // Check if the file is existing in allFileNamesft
        // If not, add its path and name into the allFileNameft
        if ( strstr(allFileNamesft, file_path) == NULL ) {
            int a = combineFileNameft (file_path, file_path + ftwbuf->base );
        }

        // Set this variable as 0 to indicate that searching is successful
        errorFLAGft = 0;

        // return 0 to make the nftw() function continue the traverse
        return 0;
    }

    // If not existing, it returns 0 to continue the traverse
    else{
        return 0;
    }
}

// Handle "w24ft" command
void handle_w24ft_ext(int conn, char *message) {

    errorFLAGft = -1;

    int argc;   // Number of arguments
    // Split the command
    char **argv = split_command(message, &argc);

    // Number of extension
    num_ext = argc - 1;

    // Initialize the extensions array
    for ( int i = 0; i < num_ext; i++ ) {
        // Allocate memory to each pointer
        extensions[i] = malloc(sizeof(char *));
        // extension is "." + argument
        sprintf(extensions[i], ".%s", argv[i+1]);
    }

    // initialize the string
   *allFileNamesft = '\0';

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkExt, 20, FTW_PHYS);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAGft == 0 ) {

            // Create the path of the TAR archive named temp.tar.gz in the home directory
            char tar_filepath[PATH_MAX];
            sprintf(tar_filepath, "%s/temp.tar.gz", home_dir);

            // Construct the shell command to compress the files into a TAR archive using "tar -czf"
            char command[100000];   // a string to store the command
            int error = 0;
            sprintf (command, "tar -czf %s%s", tar_filepath, allFileNamesft);

            // Execute the command using system()
            error = system(command);

            // If the TAR archive was created successfully, print successful message
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("TAR file created successful! The path is: \n%s\n", tar_filepath);
                sendFile(conn, tar_filepath);
            }

            // Print and send a failure message if the TAR was not created
            else {
                send(conn, "TAR file created unsuccessful!", strlen("TAR file created unsuccessful!"), 0);
                sleep_ms(200); // sleep for 200 milliseconds
                printf("TAR file created unsuccessful!\n");
            }

        }

        // The value of errorFLAG will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGft == -1 ) {
            send(conn, "No file found", strlen("No file found"), 0);
            sleep_ms(200); // sleep for 200 milliseconds
            printf("No file found\n");
        }
    }

    // nftw() returns -1 to searchResult if it detects an error and has not performed the traversal
    else if (searchResult == -1) {
        send(conn, "Error Searching", strlen("Error Searching"), 0);
        sleep_ms(200); // sleep for 200 milliseconds
        printf("Error Searching\n");
    }
}


/* Functions for "w24fda" feature */

// Create a formatted string that represents the file paths and names for archiving
// The shell command format for filepath: -C "filepath1" filename1.extension -C "filepath2" filename2.extension
// Using this format with "-C" is for only archiving the file instead of including all their directories
int combineFileNamefda ( char *filepath, char *filename ) {

    // Extract the directory path of the current filepath
    char *file_dir = dirname (filepath);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNamesfda, "%s -C \"%s\" \"%s\"", allFileNamesfda, file_dir, filename);

    // Return 1 to indicate successful completion
    return 1;
}

// Check the creation time of a filepath in the traverse
int checkDateAfter ( const char *filepath,
                     const struct stat *sb,
                     int typeflag,
                     struct FTW *ftwbuf) {

    // Copy the filepath into a new string
    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    // Convert the creation time of the current filepath into a human readable format and store it in ctime
    char ctime[15];
    strftime(ctime, sizeof(ctime), "%Y-%m-%d", localtime(&sb->st_ctime));

    // Check if the creation date of the file is larger than or equal to the start date input from client
    if (typeflag == FTW_F && strcmp(ctime, start_date) >= 0 ) {

        // Check if the file is existing in allFileNamesfda
        // If not, add its path and name into the allFileNamefda
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

// Create a TAR file that contains all the files founded in home directory, which were created after the start_date
void handle_w24fda_after(int conn, char *message) {

    errorFLAGfdb = -1;

    int argc;   // Number of arguments
    // Split the command
    char **argv = split_command(message, &argc);

    // The start_date is the second argument
    start_date = argv[1];

    *allFileNamesfda = '\0';

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkDateAfter, 20, FTW_PHYS);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAGfda == 0 ) {

            // Create the path of the TAR archive named temp.tar.gz in home directory
            char tar_filepath[PATH_MAX];
            sprintf(tar_filepath, "%s/temp.tar.gz", home_dir);

            // Construct the shell command to compress the files into a TAR archive using "tar -czf"
            char command[100000];   // a string to store the command
            int error = 0;
            sprintf (command, "tar -czf %s%s", tar_filepath, allFileNamesfda);

            // Execute the command using system()
            error = system(command);

            sleep(5);

            // If the TAR archive was created successfully, send the file
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("TAR file created successful! The path is: \n%s\n", tar_filepath);
                // Send the file to the client
                sendFile(conn, tar_filepath);
            }

            // print and send a failure message if the TAR was not created
            else {
                send(conn, "TAR file created unsuccessful!", strlen("\nTAR file created unsuccessful!"), 0);
                sleep_ms(200); // sleep for 200 milliseconds
                printf("TAR file created unsuccessful!\n");
            }
        }

        // The value of errorFLAGfda will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGfda == -1 ) {
            send(conn, "No file found", strlen("No file found"), 0);
            sleep_ms(200); // sleep for 200 milliseconds
            printf("No file found\n");
        }
    }

    // nftw() returns -1 to searchResult when it detects an error and has not performed the traversal
    else if (searchResult == -1){
        send(conn, "Error Searching", strlen("Error Searching"), 0);
        sleep_ms(200); // sleep for 200 milliseconds
        printf("Error Searching\n");
    }
}

/* Functions for "w24fdb" feature */

// Create a formatted string that represents the file paths and names for archiving
// The shell command format for filepath: -C "filepath1" filename1.extension -C "filepath2" filename2.extension
// Using this format with "-C" is for only archiving the file instead of including all their directories
int combineFileNamefdb ( char *filepath, char *filename ) {
    char quoted_path[PATH_MAX + 3];     // Store quoted directory

    // Extract the directory path of the current filepath
    char *file_dir = dirname (filepath);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNamesfdb, "%s -C \"%s\" \"%s\"", allFileNamesfdb, file_dir, filename);

    // Return 1 to indicate successful completion
    return 1;
}

// Check the creation date of the filepath
int checkDateBefore ( const char *filepath,
                      const struct stat *sb,
                      int typeflag,
                      struct FTW *ftwbuf) {

    // Copy the filepath into a new string
    char file_path[PATH_MAX];
    strcpy(file_path, filepath);

    // Convert the creation time of the current filepath into a human readable format and store it in ctime
    char ctime[15];
    strftime(ctime, sizeof(ctime), "%Y-%m-%d", localtime(&sb->st_ctime));

    // Check if the creation date of a file is as request
    if (typeflag == FTW_F && strcmp(ctime, end_date) <= 0 ) {

        // Check if the file is existing in allFileNamesfdb
        // If not, add its path and name into the allFileNamefdb
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

    errorFLAGfdb = -1;

    int argc;   // Number of arguments
    // Split the command
    char **argv = split_command(message, &argc);

    end_date = argv[1];

    *allFileNamesfdb = '\0';

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkDateBefore, 20, FTW_PHYS);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAGfdb == 0 ) {

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
                printf("TAR file created successful! The path is: \n%s\n", tar_filepath);
                sendFile(conn, tar_filepath);
            }

            // Print and send a failure message if the TAR file was not created
            else {
                send(conn, "TAR file created unsuccessful!", strlen("TAR file created unsuccessful!"), 0);
                sleep_ms(200); // sleep for 200 milliseconds
                printf("TAR file created unsuccessful!\n");
            }
        }

            // The value of errorFLAGfdb will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGfdb == -1 ) {
            send(conn, "No file found", strlen("No file found"), 0);
            sleep_ms(200); // sleep for 200 milliseconds
            printf("No file found\n");
        }
    }

        // nftw() returns -1 to searchResult when it detects an error and has not performed the traversal
    else if (searchResult == -1) {
        send(conn, "Error Searching", strlen("Error Searching"), 0);
        sleep_ms(200); // sleep for 200 milliseconds
        printf("Error Searching\n");
    }
}


// Function to handle client requests
void crequest(int conn, int server_port) {
    char message[100] = "";
    send(conn, "Server will handle the request", strlen("Server will handle the request"), 0);
    while (1) {
        // memset(message, 0, 101); // Clear message buffer
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
                sleep(1);
                memset(message, 0, 101); // Clear message buffer
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
