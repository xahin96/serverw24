#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h> // For fork
#include <string.h> // For memset

#define PORT_MIRROR2 9052

void handle_dirlist_all(int conn) {
    char message[200];
    int num_messages = 0;
    while (num_messages < 5) {
        sleep(1);
        sprintf(message, "message - %d\n", num_messages); // Create the message
        send(conn, message, strlen(message), 0);
        num_messages++;
    }
    memset(message, 0, sizeof(message)); // Clear message buffer
    // Send termination message after sending all messages
    send(conn, "END_OF_MESSAGES", strlen("END_OF_MESSAGES"), 0);
}

// Function to handle client requests
void crequest(int conn) {
    char message[100] = "";
    send(conn, "Server will handle the request", strlen("Server will handle the request"), 0);

    while (1) {
        // Receive message from client
        if (recv(conn, message, sizeof(message), 0) > 0) {
            printf("Client message from mirror1: %s\n", message);

            if (strstr(message, "dirlist -a") != NULL) {
                handle_dirlist_all(conn);
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
