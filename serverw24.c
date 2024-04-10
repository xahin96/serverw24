#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h> // For fork
#include <string.h> // For memset

#define PORT_SERVER 9050
#define PORT_MIRROR1 9051
#define PORT_MIRROR2 9052

int total_client = 0;
int server_index = 0;

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

    free(subdirs);
    return EXIT_SUCCESS;
}

// Function to handle client requests
void crequest(int conn, int server_port) {
    char message[100] = "";
    send(conn, "Server will handle the request", strlen("Server will handle the request"), 0);

    while (1) {
        // Receive message from client
        if (recv(conn, message, sizeof(message), 0) > 0) {
            printf("Client message: %s\n", message);

            if (strstr(message, "dirlist -a") != NULL) {
                handle_dirlist_alpha(conn);
            }

            if (strstr(message, "dirlist -t") != NULL) {
                handle_dirlist_time(conn);
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

void run_child_process(int fd, int conn, int server_port){
    int pid;
    if ((pid = fork()) == 0) {
        // Child process
        close(fd); // Close the listening socket in the child process
        crequest(conn, server_port); // Handle client requests and instruct client to reconnect to mirror server
        exit(EXIT_SUCCESS);
    } else if (pid == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        close(conn); // Close the connection socket in the parent process
    }
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
