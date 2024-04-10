#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h> // For fork
#include <string.h> // For memset
#include <fcntl.h>
#include <ftw.h>
#include <time.h>
#include <ctype.h>

#define PORT_MIRROR2 9052
// Function to trim trailing whitespace characters from a string
void trim_trailing_whitespace(char *str) {
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
}

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
        sleep(1);
        send(conn, subdirs[i], strlen(subdirs[i]), 0);

        // Clear message buffer
        memset(subdirs[i], 0, sizeof(subdirs[i]));
        free(subdirs[i]);
    }
    
    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    free(subdirs);
    return EXIT_SUCCESS;
}

// Get all the subdirectories in home directory in order of creation time
char** getSubdirectories_time(int *count) {
    FILE *fp;   // file pointer
    char dir_name[1035];    // name of a directory
    char **subdirs = NULL;  // array of pointers referring to all the subdirectories
    int subdir_count = 0;   // number of subdirectories, initialized as 0

    // Run the shell command and its output will be available for reading from 'fp'
    // This command lists all the directories in the user's home directory in the order of creation time
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

// Handle "dirlist -t" command 
int handle_dirlist_time(int conn) {
    int num_subdir;
    char **subdirs = getSubdirectories_time(&num_subdir);
    if (subdirs == NULL) {
        fprintf(stderr, "Error: Failed to get subdirectories.\n");
        return EXIT_FAILURE;
    }

    char message[200];
    sprintf(message, "There are %d subdirectories:\n", num_subdir);
    send(conn, message, strlen(message), 0);

    // Send directories to the client one by one
    for (int i = 0; i < num_subdir; i++) {
        sleep(1);
        send(conn, subdirs[i], strlen(subdirs[i]), 0);

        // Clear message buffer
        memset(subdirs[i], 0, sizeof(subdirs[i]));
        free(subdirs[i]);
    }
    
    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);

    memset(message, 0, sizeof(message));

    free(subdirs);
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
int checkFirst ( const char *filepath,
                 const struct stat *sb,
                 int typeflag,
                 struct FTW *ftwbuf ) {

    // Check if the input file existing in the traversed path
    if (typeflag == FTW_F && strstr(filepath, filename) != NULL) {

        // printf("File: %s\n", filename);
        snprintf(name_message, sizeof(name_message), "\nFile: %s\n", filename);
        // printf("Size: %ld bytes\n", sb->st_size);
        snprintf(size_message, sizeof(size_message), "Size: %ld bytes\n", sb->st_size);
        // printf("Date of Creation: %s\n", ctime(&sb->st_ctime));
        snprintf(date_message, sizeof(date_message), "Date of Creation: %s", ctime(&sb->st_ctime));
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

int handle_w24fn_filename(int conn, char *message) {

    // Trim the white space after the message
    // trim_trailing_whitespace(message);

    int argc;
    char **argv = split_command(message, &argc);

    filename = argv[1];
    // printf("file name : %s\n", filename);

    const char *home_dir = getenv("HOME");   
    // const char *home_dir = "/home/song59/Desktop/asp/shellscript";   // for testing

    // Traverse all files in the home directory, the second argument
    int searchResult = nftw(home_dir, checkFirst, 20, FTW_NS);

    // nftw() returns 1 if the callback function checkFirst() returns 1
    // Which means the file has been found
    if ( searchResult > 0 ){
        send(conn, name_message, strlen(name_message), 0);
        // printf("File: %s\n", filename);
        sleep(1);

        send(conn, size_message, strlen(size_message), 0);
        // printf("%s\n", size_message);
        sleep(1);
        
        send(conn, date_message, strlen(date_message), 0);
        // printf("%s", date_message);
        sleep(1);

        send(conn, permission_message, strlen(permission_message), 0);
        // printf("%s\n", permission_message);
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

    memset(filename, 0, sizeof(filename));
    memset(size_message, 0, sizeof(size_message));
    memset(date_message, 0, sizeof(date_message));
    memset(permission_message, 0, sizeof(permission_message));

    return EXIT_SUCCESS;
}

// Function to handle client requests
void crequest(int conn) {
    char message[100] = "";
    send(conn, "Mirror 2 will handle the request", strlen("Mirror 2 will handle the request"), 0);

    while (1) {
        // Receive message from client
        if (recv(conn, message, sizeof(message), 0) > 0) {
            // Trim the white space after the message
            trim_trailing_whitespace(message);
            
            printf("Client message for mirror 2: %s\n", message);

            if (strstr(message, "dirlist -a") != NULL) {
                handle_dirlist_alpha(conn);
            }

            if (strstr(message, "dirlist -t") != NULL) {
                handle_dirlist_time(conn);
            }

            if (strstr(message, "w24fn") != NULL) {
                handle_w24fn_filename(conn, message);
            }

            // Check for exit condition
            if (strncmp(message, "quitc", 5) == 0) {
                break;
            }
        }
    }
    close(conn); // Close the connection socket
}

int main() {
    struct sockaddr_in serv;
    int fd, conn;

    // Initialize socket structure
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT_MIRROR2);
    serv.sin_addr.s_addr = INADDR_ANY;

    // Create socket
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to address
    if (bind(fd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(fd, 15) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept and handle connections
    while ((conn = accept(fd, (struct sockaddr *)NULL, NULL))) {
        int pid;
        if ((pid = fork()) == 0) {
            // Child process
            close(fd); // Close the listening socket in the child process
            crequest(conn); // Handle client requests
            exit(EXIT_SUCCESS);
        } else if (pid == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            close(conn); // Close the connection socket in the parent process
        }
    }

    if (conn == -1) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}
