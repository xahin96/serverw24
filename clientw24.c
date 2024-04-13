#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h> // For inet_pton
#include <string.h> // For strlen
#include <unistd.h>
#include <ifaddrs.h>

#define BUFFER_SIZE 32768


#define DEST_FILE "destination/file.tar.gz"

void handle_dirlist_alpha(int fd) {
    char message[100] = "";
    int end_of_messages_received = 0; // Flag to indicate whether "END_OF_MESSAGES" has been received
    while (!end_of_messages_received) {
        // Receive message from server
        memset(message, 0, sizeof(message)); // Clear message buffer
        int bytes_received = recv(fd, message, sizeof(message), 0);
        if (bytes_received > 0) {
            printf("%s\n", message);
            if (strstr(message, "END_OF_MESSAGES") != NULL) {
                end_of_messages_received = 1; // Set flag to true
            }
        } else if (bytes_received == 0) {
            break; // Terminate loop when connection closed
        } else {
            // Handle the case where recv returns -1 (indicating error)
            perror("recv");
            break;
        }
    }
}

void handle_dirlist_time(int fd) {
    char message[100] = "";
    int end_of_messages_received = 0; // Flag to indicate whether "END_OF_MESSAGES" has been received
    while (!end_of_messages_received) {
        // Receive message from server
        memset(message, 0, sizeof(message)); // Clear message buffer
        int bytes_received = recv(fd, message, sizeof(message), 0);
        if (bytes_received > 0) {
            printf("%s\n", message);
            if (strstr(message, "END_OF_MESSAGES") != NULL) {
                end_of_messages_received = 1; // Set flag to true
            }
        } else if (bytes_received == 0) {
            break; // Terminate loop when connection closed
        } else {
            // Handle the case where recv returns -1 (indicating error)
            perror("recv");
            break;
        }
    }
}

void handle_w24fn_filename(int fd) {
    char message[100] = "";
    int end_of_messages_received = 0; // Flag to indicate whether "END_OF_MESSAGES" has been received
    while (!end_of_messages_received) {
        // Receive message from server
        memset(message, 0, sizeof(message)); // Clear message buffer
        int bytes_received = recv(fd, message, sizeof(message), 0);
        if (bytes_received > 0) {
            printf("%s\n", message);
            if (strstr(message, "END_OF_MESSAGES") != NULL) {
                end_of_messages_received = 1; // Set flag to true
            }
        } else if (bytes_received == 0) {
            break; // Terminate loop when connection closed
        } else {
            // Handle the case where recv returns -1 (indicating error)
            perror("recv");
            break;
        }
    }
}


void handle_w24fz_all(int fd) {
    // Receive file size string
    char file_size_str[20]; // Assuming a maximum of 20 digits for the file size
    memset(file_size_str, 0, sizeof(file_size_str)); // Clear message buffer
//    recv(fd, file_size_str, sizeof(file_size_str) - 1, 0);
    recv(fd, file_size_str, sizeof(file_size_str), 0);
    recv(fd, file_size_str, sizeof(file_size_str), 0);
    recv(fd, file_size_str, sizeof(file_size_str), 0);
    recv(fd, file_size_str, sizeof(file_size_str), 0);
    recv(fd, file_size_str, sizeof(file_size_str), 0);
    recv(fd, file_size_str, sizeof(file_size_str), 0);
    recv(fd, file_size_str, sizeof(file_size_str), 0);
    recv(fd, file_size_str, sizeof(file_size_str), 0);

    printf("file size from client str: %s \n", file_size_str);


    // Convert file size string to long
    long file_size = atol(file_size_str);

    printf("file size from client: %ld \n", file_size);

    // Open destination file for writing
    FILE *file = fopen(DEST_FILE, "wb");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Receive file data and write to destination file
    size_t total_bytes_received = 0;
    size_t bytes_received;
    char buffer[BUFFER_SIZE];
    while (total_bytes_received < file_size) {
        memset(buffer, 0, sizeof(buffer)); // Clear message buffer
        bytes_received = recv(fd, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }
        total_bytes_received += bytes_received;
        fwrite(buffer, 1, bytes_received, file);
    }

    printf("File received successfully.\n");
    fclose(file);
}


int main() {
    // char *ipAddress = getIPAddress();    // Mac
    char *ipAddress = "127.0.0.1";          // Linux

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
    serv.sin_port = htons(9050);
    if (inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(fd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    } else {
        if (recv(fd, message, sizeof(message), 0) > 0) {
            // Check if the server instructs to connect to a different port
            if (strncmp(message, "CONNECT_TO_PORT:", 16) == 0) {
                int port = atoi(message + 16); // Extract port number from the message
                printf("Connecting to port %d\n", port);
                close(fd); // Close current connection
                // Reconnect to the specified port
                serv.sin_port = htons(port);
                if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                    perror("Socket creation failed");
                    exit(EXIT_FAILURE);
                }
                if (connect(fd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
                    perror("Connection failed");
                    exit(EXIT_FAILURE);
                }
            } else {
                // Normal message received from the server
                printf("Server: %s\n", message);
            }
            memset(message, 0, sizeof(message)); // Clear message buffer
        }
    }

    while (1) {
        printf("Client sent 'w24fz' message\n");

        // Send message to server
        printf("clientw24$ ");
        fgets(message, sizeof(message), stdin);
        if (strstr(message, "dirlist -a") != NULL) {
            send(fd, message, strlen(message), 0);
            handle_dirlist_alpha(fd);
        } else if (strstr(message, "dirlist -t") != NULL) {
            send(fd, message, strlen(message), 0);
            handle_dirlist_time(fd);
        } else if (strstr(message, "w24fn") != NULL) {
            send(fd, message, strlen(message), 0);
            handle_w24fn_filename(fd);
        } else if (strstr(message, "w24fz") != NULL) {
            send(fd, message, strlen(message), 0);
            handle_w24fz_all(fd);
        } else if (strncmp(message, "quitc", 5) == 0) {
            break;
        } else {
            send(fd, message, strlen(message), 0);
        }
    }

    // Close the socket
    close(fd);

    return 0;
}
