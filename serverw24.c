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
#include <libgen.h>
#include <err.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>


#define BUFFER_SIZE 32768
#define PORT_SERVER 9050
#define PORT_MIRROR1 9051
#define PORT_MIRROR2 9052


#define FILENAME "source/file.tar.gz"


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
        sleep(1);
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
int checkFirst ( const char *filepath,
                 const struct stat *sb,
                 int typeflag,
                 struct FTW *ftwbuf ) {

    // Check if the input file existing in the traversed path
    if (typeflag == FTW_F && strstr(filepath, filename) != NULL) {

        // printf("File: %s\n", filename);
        printf("\nFile: %s\n", filename);
        // printf("Size: %ld bytes\n", sb->st_size);
        printf("Size: %ld bytes\n", sb->st_size);
        // printf("Date of Creation: %s\n", ctime(&sb->st_ctime));
        printf("Date of Creation: %s", ctime(&sb->st_ctime));
        printf("Permissions: %c%c%c%c%c%c%c%c%c%c\n",
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


void handle_w24fz_all(int conn) {
    // Open the file to send
    FILE *file = fopen(FILENAME, "rb");
    if (!file) {
        perror("Error opening file");
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
    sleep_ms(100); // sleep for 1000 milliseconds (1 second)

    send(conn, file_size_str, strlen(file_size_str), 0);
    sleep_ms(100); // sleep for 1000 milliseconds (1 second)

    send(conn, file_size_str, strlen(file_size_str), 0);
    sleep_ms(100); // sleep for 1000 milliseconds (1 second)

    send(conn, file_size_str, strlen(file_size_str), 0);
    sleep_ms(100); // sleep for 1000 milliseconds (1 second)

    send(conn, file_size_str, strlen(file_size_str), 0);
    sleep_ms(100); // sleep for 1000 milliseconds (1 second)

    send(conn, file_size_str, strlen(file_size_str), 0);
    sleep_ms(100); // sleep for 1000 milliseconds (1 second)

    send(conn, file_size_str, strlen(file_size_str), 0);
    sleep_ms(100); // sleep for 1000 milliseconds (1 second)

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
                handle_w24fz_all(conn);
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

        // Send "Server will handle the request" message
        send(fd_client, "Server will handle the request", strlen("Server will handle the request"), 0);

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
