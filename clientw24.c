#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h> // For inet_pton
#include <string.h> // For strlen
#include <unistd.h>
#include <ifaddrs.h>
#include <ctype.h>


#define BUFFER_SIZE 32768


#define DEST_FILE "./temp.tar.gz"

// For counting the total number of space
int special_space_count = 0;
#define MAX_COMMAND_LENGTH 100

// Function to trim trailing whitespace characters from a string
void trim_trailing_whitespace(char *str) {
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
}

// Splits a command with space and returns the sub command list
char **split_by_space_operator(const char *command, const char *special_character) {
    // restarting the space count variable
    special_space_count = 0;
    // variable for storing the temporary sub command
    char *sub_command_string;
    // Copying the command so that the main string doesn't get manipulated
    char *command_copy = strdup(command);
    // array for storing all the sub commands
    char **commands = malloc(MAX_COMMAND_LENGTH * sizeof(char *));

    // tokenizing the command_copy variable by special_character
    sub_command_string = strtok(command_copy, special_character);
    // looking for all the tokens for storing
    while (sub_command_string != NULL) {
        // inserting sub command to array
        commands[special_space_count++] = strdup(sub_command_string);
        // continuing the sub command splitting
        sub_command_string = strtok(NULL, special_character);
    }
    // null terminating for ending the array
    commands[special_space_count] = NULL;
    // freeing the memory
    free(command_copy);
    return commands;
}

void handle_dirlist_alpha(int fd) {
    char message[100] = "";
    int end_of_messages_received = 0; // Flag to indicate whether "END_OF_MESSAGES" has been received
    while (!end_of_messages_received) {
        // Receive message from server
        memset(message, 0, 101); // Clear message buffer
        int bytes_received = recv(fd, message, sizeof(message), 0);
        if (bytes_received > 0) {
            if (strstr(message, "END_OF_MESSAGES") != NULL) {
                end_of_messages_received = 1; // Set flag to true
                continue;
            }
            printf("%s\n", message);
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
        memset(message, 0, 101); // Clear message buffer
        int bytes_received = recv(fd, message, sizeof(message), 0);
        if (bytes_received > 0) {
            if (strstr(message, "END_OF_MESSAGES") != NULL) {
                end_of_messages_received = 1; // Set flag to true
                continue;
            }
            printf("%s\n", message);
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
        memset(message, 0, 101); // Clear message buffer
        int bytes_received = recv(fd, message, sizeof(message), 0);
        // printf("bytes_received %d\n", bytes_received);

        if (bytes_received > 0) {
            if (strstr(message, "END_OF_MESSAGES") != NULL) {
                end_of_messages_received = 1; // Set flag to true
                continue;
            } else if (strstr(message, "NON_FILE")) {
                printf("File not found\n");
                continue;
            }
            printf("%s\n", message);
        } else if (bytes_received == 0) {
            break; // Terminate loop 4hen connection closed
        } else {
            // Handle the case where recv returns -1 (indicating error)
            perror("recv");
            break;
        }
    }
}


void handle_w24fz_size(int fd) {
    // Receive file size string
    char file_size_str[16]; // Assuming a maximum of 20 digits for the file size
    memset(file_size_str, 0, 17); // Clear message buffer
    recv(fd, file_size_str, 16, 0);
    if (strstr(file_size_str, "No file found") != NULL) {
        printf("%s\n", file_size_str);
    } else {
        // Convert file size string to long
        long file_size = atol(file_size_str);

        printf("file size from client: %ld \n", file_size);
        memset(file_size_str, 0, sizeof(file_size_str)); // Clear message buffer

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
}

void handle_w24ft_ext(int fd) {
    // Receive file size string
    char file_size_str[13]; // Assuming a maximum of 20 digits for the file size
    memset(file_size_str, 0, 14); // Clear message buffer
    recv(fd, file_size_str, 13, 0);

    if (strstr(file_size_str, "No file found") != NULL) {
        // If no file found, print the message received from server
        printf("%s\n", file_size_str);
    } else {
        // Convert file size string to long
        long file_size = atol(file_size_str);

        memset(file_size_str, 0, sizeof(file_size_str)); // Clear message buffer


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
}

void handle_w24fda_after(int fd) {
    // Receive file size string
    char file_size_str[13]; // Assuming a maximum of 20 digits for the file size
    memset(file_size_str, 0, 14); // Clear message buffer
    recv(fd, file_size_str, 13, 0);

    if (strstr(file_size_str, "No file found") != NULL) {
        printf("%s\n", file_size_str);
    } else {
        // Convert file size string to long
        long file_size = atol(file_size_str);

        printf("file size from client: %ld \n", file_size);
        memset(file_size_str, 0, strlen(file_size_str)); // Clear message buffer

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
            fclose(file);
        }
        printf("File received successfully.\n");
    }
}

void handle_w24fdb_before(int fd) {
    // w24fdb 2022-01-01
    // Receive file size string
    char file_size_str[13]; // Assuming a maximum of 20 digits for the file size
    memset(file_size_str, 0, 20); // Clear message buffer
    recv(fd, file_size_str, 13, 0);
    if (strstr(file_size_str, "No file found") != NULL) {
        // If no file found, print the message received from server
        printf("%s\n", file_size_str);
    } else {
        // Convert file size string to long
        long file_size = atol(file_size_str);

        memset(file_size_str, 0, sizeof(file_size_str)); // Clear message buffer

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
}

bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

bool isNumber(char *str) {
    char number[255];
    strcpy(number, str);

    char *number_ptr = number;
    while (*number_ptr) {
        if (!isdigit(*number_ptr) && *number_ptr != '\n') {
            return false;
        }
        number_ptr++;
    }

    return true;
}

bool validate_date(char **specific_command) {
    const char *date_str = specific_command[1];

    char date[255];
    strcpy(date, date_str);

    char *date_ptr = date;

    // while (*date_ptr != '\n') {

    //     if (!isdigit(*date_ptr) && *date_ptr != '-') {
    //                     printf("false 1 %s\n", date_ptr );

    //         printf("false 1\n");
    //         return false;
    //     }
    //     date_ptr++;
    // }

    int year, month, day;
    if (sscanf(date_str, "%d-%d-%d", &year, &month, &day) != 3)
    {
        printf("false 2\n");
        return false;
    }

    if (year < 999 || year > 9999){
        printf("false 3\n");
        return false;
    }

    if (month < 1 || month > 12)
    {
        printf("false 4\n");
        return false;
    }

    int days_in_month;
    switch (month) {
        case 2:
            days_in_month = 28 + is_leap_year(year);
            break;
        case 4: case 6: case 9: case 11:
            days_in_month = 30;
            break;
        default:
            days_in_month = 31;
    }

    if (day < 1 || day > days_in_month)
    {
        printf("false 5\n");
        return false;
    }


    return true;
}

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
    serv.sin_port = htons(9059);
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
            memset(&message, 0, strlen(message)); // Clear message buffer
        }
    }

    while (1) {

        // Send message to server
        printf("clientw24$ ");
        fgets(message, sizeof(message), stdin);
        trim_trailing_whitespace(message);
        if (strstr(message, "dirlist -a") != NULL) {
            if (strlen(message) == 10)
            {
                send(fd, message, strlen(message), 0);
                handle_dirlist_alpha(fd);
            } else {
                printf("Invalid command.\n");
            }
        } else if (strstr(message, "dirlist -t") != NULL) {
            if (strlen(message) == 10)
            {
                send(fd, message, strlen(message), 0);
                handle_dirlist_time(fd);
            } else {
                printf("Invalid command.\n");
            }
        } else if (strstr(message, "w24fn") != NULL) {
            char **specific_command = split_by_space_operator(message, " ");
            if (special_space_count == 2 && strlen(specific_command[0]) == 5) {
                send(fd, message, strlen(message), 0);
                handle_w24fn_filename(fd);
                free(specific_command);
            } else {
                printf("Invalid 'w24fn' command. Command structure:\nw24fn [file name]\n");
                free(specific_command);
            }
        } else if (strstr(message, "w24fz") != NULL) {
            int min_search_size;
            int max_search_size;
            char **specific_command = split_by_space_operator(message, " ");
            if (special_space_count == 3 && strlen(specific_command[0]) == 5 && isNumber(specific_command[1]) && isNumber(specific_command[2])) {
                min_search_size = atoi(specific_command[1]);
                max_search_size = atoi(specific_command[2]);
                if (min_search_size >= 0 && max_search_size >= 0) {
                    if (min_search_size <= max_search_size){
                        send(fd, message, strlen(message), 0);
                        handle_w24fz_size(fd);
                    } else {
                        printf("size2 must be greater than or equal to size1\n");
                    }
                } else {
                    printf("size1 and size2 must be greater than or equal to zero\n");
                }
                free(specific_command);
            } else {
                printf("Invalid 'w24fz' command. Command structure:\nw24fz [size1] [size2]\n");
                free(specific_command);
            }
        } else if (strstr(message, "w24ft") != NULL) {
            char **specific_command = split_by_space_operator(message, " ");
            if (special_space_count >= 2 && special_space_count <= 4 && strlen(specific_command[0]) == 5) {
                send(fd, message, strlen(message), 0);
                handle_w24ft_ext(fd);
                free(specific_command);
            } else {
                printf("Invalid 'w24ft' command. Command structure:\nw24ft [file extension 1]\nor\nw24ft [file extension 1] [file extension 2]\nor\nw24ft [file extension 1] [file extension 2] [file extension 3]\n");
                free(specific_command);
            }
        } else if (strstr(message, "w24fda") != NULL) {
            char **specific_command = split_by_space_operator(message, " ");
            if (special_space_count == 2 && validate_date(specific_command) && strlen(specific_command[0]) == 6) {
                send(fd, message, strlen(message), 0);
                handle_w24fda_after(fd);
            } else {
                printf("Invalid 'w24fda' command. Command structure:\nw24fda [Date(yyyy-mm-dd)]\n\n");
                free(specific_command);
            }
        } else if (strstr(message, "w24fdb") != NULL) {
            char **specific_command = split_by_space_operator(message, " ");
            if (special_space_count == 2 && validate_date(specific_command) && strlen(specific_command[0]) == 6) {
                send(fd, message, strlen(message), 0);
                handle_w24fdb_before(fd);
            } else {
                printf("Invalid 'w24fdb' command. Command structure:\nw24fdb [Date(yyyy-mm-dd)]\n\n");
                free(specific_command);
            }
        } else if (strstr(message, "quitc") != NULL) {
            if (strlen(message) == 5) {
                send(fd, message, strlen(message), 0);
                close(fd);
                break;
            } else {
                printf("Invalid command.\n");
            }
        } else {
            printf("Invalid command\n");
        }
    }

    // Close the socket
    close(fd);

    return 0;
}
