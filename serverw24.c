#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h> // For fork
#include <string.h> // For memset


void serve_user_request(int fd) {
    char * message = "Received";
    send(fd, message, strlen(message), 0);
}


// Function to handle client requests
void crequest(int conn) {
    char message[100] = "";

    while (1) {
        // Receive message from client
        if (recv(conn, message, sizeof(message), 0) > 0) {
            printf("Client: %s\n", message);
            memset(message, 0, sizeof(message)); // Clear message buffer
        }

        // Perform actions based on client command
        serve_user_request(conn);

        // Check for exit condition
        if (strncmp(message, "quitc", 5) == 0) {
            printf("Exiting crequest()...\n");
            break;
        }
    }
    close(conn); // Close the connection socket
}

int main() {
    struct sockaddr_in serv;
    int fd, conn;

    // Initialize socket structure
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8096);
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
    if (listen(fd, 5) == -1) {
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
