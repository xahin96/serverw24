#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h> // For fork
#include <string.h> // For memset
#include <arpa/inet.h> // For inet_pton
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


int total_client = 0;
int server_index = 0;


void serve_user_request(int fd) {
    char * message = "Received by serverw24";
    send(fd, message, strlen(message), 0);
}


// Function to handle client requests
void crequest(int conn) {
    char message[100] = "";

    while (1) {
        // Receive message from client
        if (recv(conn, message, sizeof(message), 0) > 0) {
            printf("Client message from server: %s\n", message);
            // Check for exit condition
            if (strncmp(message, "quitc", 5) == 0) {
                printf("Exiting crequest()...\n");
                break;
            }
        }

        // Perform actions based on client command
        serve_user_request(conn);

    }
    close(conn); // Close the connection socket
}


void run_child_process(int fd, int conn){
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


int main() {
    char *ipAddress = getIPAddress();

    struct sockaddr_in serverw24;
    int fd, conn;

    // Initializing serverw24 socket structure
    serverw24.sin_family = AF_INET;
    serverw24.sin_port = htons(9050);
    serverw24.sin_addr.s_addr = INADDR_ANY;


    free(ipAddress);

    // Create socket
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to address
    if (bind(fd, (struct sockaddr *)&serverw24, sizeof(serverw24)) == -1) {
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

        if (total_client < 3) {
            // Increment total_client count and print
            printf("Client handled by server\n");
            run_child_process(fd, conn);
        } else if (total_client < 6) {
            printf("Client handled by mirror1\n");
        } else if (total_client < 9) {
            printf("Client handled by mirror2\n");
        } else {
            // Cycle through server, mirror1, and mirror2
            if (server_index == 0) {
                printf("Client handled by server\n");
                run_child_process(fd, conn);
            } else if (server_index == 1) {
                printf("Client handled by mirror1\n");
            } else {
                printf("Client handled by mirror2\n");
            }
            server_index = (server_index + 1) % 3;
        }
        total_client++;
    }

    if (conn == -1) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}
