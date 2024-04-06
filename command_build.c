#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_NUM_COMMANDS 6     // up to 5 operations be supported

// Split each command of userInput into argv
char **split_command (char *clientCommands, int *argc) {

    // Copy the command into another pointer
    char *command_copy = strdup(clientCommands);

    // Reset the number of arguments as 0
    *argc = 0;
    char **argv = malloc(sizeof(char *) * 6);

    // Get the first token from userInput, which is the name of the command
    char *token = strtok (command_copy, " ");

    while ( token != NULL ) {

        // Duplicate the token to argv[]
        argv[*argc] = strdup (token);
        (*argc)++;

        // Split userInput into arguments based on space " "
        token = strtok(NULL, " ");
    }

    // Free the memory allocated for command_copy
    free(command_copy);
    return argv;
}

char **dirlist_alphab () {

}

char **dirlist_time () {

}

// Check the userInput string
void check_commands(char *userInput) {

    int argc;
    char **argv = split_command(userInput, &argc);

    // "dirlist"
    if ( strcmp( argv[0], "dirlist" ) == 0 ) {
        if (strcmp(argv[1], "-a" ) == 0 ) {

            printf("argc = %d\n", argc);
            for (int i = 0; i < argc; i++) {
                printf("argv[ %d ] = %s\n", i, argv[i]);
            }
//            dirlist_alphab();
        }
        else if ( strcmp( argv[1], "-t" ) == 0 ) {
            printf("argc = %d\n", argc);
            for (int i = 0; i < argc; i++) {
                printf("argv[ %d ] = %s\n", i, argv[i]);
            }
//            dirlist_time();

        }
        else {
            printf("\ninvalid input\n");
        }
    }

    free(argv);
}

int main() {

    /* Append the current working directory to PATH so that the executables in this directory
    can be run without specifying their path */
//    append_cwd();

    char userInput[10000];  // storing the string input by user

    // Infinite loop
    while(1) {

        fflush(stdout);
        fflush(stdin);

        // Waiting for user's input
        printf("\nclientw24$ ");

        // read a string input by users until a newline '\n' is input
        /* the space before '%' make 'scanf' to ignore any leading whitespace
            including spaces, tabs, and newlines before reading */
        scanf(" %[^\n]", userInput);

        check_commands(userInput);

//        free(userInput);
//        userInput[0] = '\0';
    }

    return 0;
}