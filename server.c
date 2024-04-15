#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 9060
#define FILENAME "source/file.tar.gz"

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Infinite loop to accept connections and handle commands
    while(1) {
        printf("Waiting for client connection...\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("Client connected.\n");

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

        // Send file size to client
        send(new_socket, &file_size, sizeof(file_size), 0);

        // Send file data to client
        char buffer[1024] = {0};
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            if (send(new_socket, buffer, bytes_read, 0) != bytes_read) {
                perror("send");
                exit(EXIT_FAILURE);
            }
        }

        fclose(file);
        close(new_socket);
        printf("File sent successfully.\n");
    }
    close(server_fd);
    return 0;
}
