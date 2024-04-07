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
    serv.sin_port = htons(9050); // Always connect to port 9050 initially
    if (inet_pton(AF_INET, ipAddress, &serv.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }
    free(ipAddress);

    // Connect to the server
    if (connect(fd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send and receive messages to/from the server
    while (1) {
        // Receive message from server
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

        // Send message to server
        printf("Enter your message: ");
        fgets(message, sizeof(message), stdin);
        send(fd, message, strlen(message), 0);
        if (strncmp(message, "quitc", 5) == 0) {
            printf("Exiting client...\n");
            break;
        }
        memset(message, 0, sizeof(message)); // Clear message buffer
    }

    // Close the socket
    close(fd);

    return 0;
}
