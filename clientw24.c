#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h> // For inet_pton
#include <string.h> // For strlen
#include <unistd.h>
#include <ifaddrs.h>


#ifdef __APPLE__
#define INTERFACE_NAME "en0" // Interface name for macOS
#elif __linux__
#define WLAN_INTERFACE_PREFIX "wlan" // Interface name prefix for Linux Wi-Fi
#define ETH_INTERFACE_PREFIX "eth"   // Interface name prefix for Linux Ethernet
#else
#error "Unsupported OS"
#endif

char *getIPAddress() {
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char *ipAddress = NULL;

    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

#ifdef __APPLE__
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            sa = (struct sockaddr_in *) ifa->ifa_addr;
            if (strcmp(ifa->ifa_name, INTERFACE_NAME) == 0) {
                ipAddress = strdup(inet_ntoa(sa->sin_addr));
                break;
            }
        }
    }
#elif __linux__
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            sa = (struct sockaddr_in *) ifa->ifa_addr;
            if (strstr(ifa->ifa_name, WLAN_INTERFACE_PREFIX) != NULL || strstr(ifa->ifa_name, ETH_INTERFACE_PREFIX) != NULL) {
                ipAddress = strdup(inet_ntoa(sa->sin_addr));
                break;
            }
        }
    }
#endif

    freeifaddrs(ifap);
    return ipAddress;
}


void handle_dirlist_all(int fd) {
    char message[100] = "";
    int end_of_messages_received = 0; // Flag to indicate whether "END_OF_MESSAGES" has been received
    while (!end_of_messages_received) {
        // Receive message from server
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


int main() {
    char *ipAddress = getIPAddress();
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
    serv.sin_port = htons(9051); // Always connect to port 9051 initially
    if (inet_pton(AF_INET, ipAddress, &serv.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(fd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Send message to server
        printf("clientw24$ ");
        fgets(message, sizeof(message), stdin);
        if (strstr(message, "dirlist -a") != NULL) {
            send(fd, message, strlen(message), 0);
            handle_dirlist_all(fd);
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
