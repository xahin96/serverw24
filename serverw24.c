#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h> // For fork
#include <string.h> // For memset
#include <time.h>


#define BUFFER_SIZE 1024
#define PORT_SERVER 9050
#define PORT_MIRROR1 9051
#define PORT_MIRROR2 9052


#define FILENAME "source/file.tar.gz"


int total_client = 0;
int server_index = 0;


void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    while (nanosleep(&ts, &ts) == -1);
}


void handle_dirlist_all(int conn) {
    char message[200];
    int num_messages = 0;
    while (num_messages < 5) {
        sleep_ms(100); // sleep for 1000 milliseconds (1 second)
        sprintf(message, "message - %d", num_messages); // Create the message
        send(conn, message, strlen(message), 0);
        num_messages++;
    }
    memset(message, 0, sizeof(message)); // Clear message buffer
    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);
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

    // Send file data to client
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        printf("bytes found \n");
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
    send(conn, "Server will handle the request\n", strlen("Server will handle the request\n"), 0);

    while (1) {
        // Receive message from client
        if (recv(conn, message, sizeof(message), 0) > 0) {
            printf("Client message: %s\n", message);

            if (strstr(message, "dirlist -a") != NULL) {
                handle_dirlist_all(conn);
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
        send(fd_client, "Server will handle the request\n", strlen("Server will handle the request\n"), 0);

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
