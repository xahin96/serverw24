#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 9060
#define DEST_FILE "destination/file.tar.gz"

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    while(1) {
        printf("Type 'send' to receive file: ");
        fgets(buffer, sizeof(buffer), stdin);
        strtok(buffer, "\n"); // Remove newline character

        // Send command to server
        send(sock, buffer, strlen(buffer), 0);

        // Receive file size
        long file_size;
        recv(sock, &file_size, sizeof(file_size), 0);

        // Open destination file for writing
        FILE *file = fopen(DEST_FILE, "wb");
        if (!file) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

        // Receive file data and write to destination file
        size_t total_bytes_received = 0;
        size_t bytes_received;
        while (total_bytes_received < file_size) {
            bytes_received = recv(sock, buffer, sizeof(buffer), 0);
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

    close(sock);
    return 0;
}
