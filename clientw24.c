#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h> // For inet_pton
#include <string.h> // For strlen
#include <unistd.h>

int main() {
    struct sockaddr_in serv;
    int fd;
    char message[100] = "";

    // Create socket
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8096);
    if (inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(fd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send and receive messages to/from the server
    while (1) {
        // Send message to server
        printf("Enter your message: ");
        fgets(message, sizeof(message), stdin);
        send(fd, message, strlen(message), 0);
        memset(message, 0, sizeof(message)); // Clear message buffer

        // Receive message from server
        if (recv(fd, message, sizeof(message), 0) > 0) {
            printf("Server: %s\n", message);
            memset(message, 0, sizeof(message)); // Clear message buffer
        }

        // Check for exit condition
        if (strncmp(message, "exit", 4) == 0) {
            printf("Exiting...\n");
            break;
        }
    }

    // Close the socket
    close(fd);

    return 0;
}
