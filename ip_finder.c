#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __APPLE__
#define INTERFACE_NAME "en0" // Interface name for macOS
#elif __linux__
#define WLAN_INTERFACE_PREFIX "wlan" // Interface name prefix for Linux Wi-Fi
#define ETH_INTERFACE_PREFIX "eth"   // Interface name prefix for Linux Ethernet
#else
#error "Unsupported OS"
#endif

int main() {
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;

    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

#ifdef __APPLE__
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            sa = (struct sockaddr_in *) ifa->ifa_addr;
            if (strcmp(ifa->ifa_name, INTERFACE_NAME) == 0) {
                printf("IPv4 Address: %s\n", inet_ntoa(sa->sin_addr));
                break;
            }
        }
    }
#elif __linux__
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            sa = (struct sockaddr_in *) ifa->ifa_addr;
            if (strstr(ifa->ifa_name, WLAN_INTERFACE_PREFIX) != NULL || strstr(ifa->ifa_name, ETH_INTERFACE_PREFIX) != NULL) {
                printf("IPv4 Address for interface %s: %s\n", ifa->ifa_name, inet_ntoa(sa->sin_addr));
            }
        }
    }
#endif

    freeifaddrs(ifap);
    return 0;
}
