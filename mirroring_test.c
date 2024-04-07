#include <stdio.h>

int main() {
    int total_client = 0;
    int server_index = 0;

    for (int i = 0; i < 30; i++) {
        if (total_client < 3) {
            printf("Client %d handled by server\n", i + 1);
        } else if (total_client < 6) {
            printf("Client %d handled by mirror1\n", i + 1);
        } else if (total_client < 9) {
            printf("Client %d handled by mirror2\n", i + 1);
        } else {
            // Cycle through server, mirror1, and mirror2
            if (server_index == 0) {
                printf("Client %d handled by server\n", i + 1);
            } else if (server_index == 1) {
                printf("Client %d handled by mirror1\n", i + 1);
            } else {
                printf("Client %d handled by mirror2\n", i + 1);
            }
            server_index = (server_index + 1) % 3;
        }
        total_client++;
    }

    return 0;
}
