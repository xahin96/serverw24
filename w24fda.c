#define _XOPEN_SOURCE 500 // Required for nftw

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h> // For fork
#include <string.h> // For memset
#include <time.h>
#include <fcntl.h>
#include <ftw.h>
#include <ctype.h>
#include <libgen.h>
#include <err.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>

char *start_date;
int errorFLAGfda = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNamesfda[100000];      // store all the file paths and names for tar command


// Create a formatted string that represents the file paths and names for archiving
// The shell command format for filepath: -C "filepath1" filename1.extension -C "filepath2" filename2.extension
// Using this format with "-C" is for only archiving the file instead of including all their directories
int combineFileNamefda ( const char *filepath, const char *filename ) {

    // Extract the directory path of the current filepath
    char *file_dir = dirname (filepath);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNamesfda, "%s -C \"%s\" \"%s\"", allFileNamesfda, file_dir, filename);

    // Return 1 to indicate successful completion
    return 1;
}

int checkDateAfter ( const char *filepath,
                  const struct stat *sb,
                  int typeflag,
                  struct FTW *ftwbuf) {
    
    char file_path[PATH_MAX];
    strcpy(file_path, filepath);
    printf("file_path = %s\n", file_path);

    char ctime[15];
    strftime(ctime, sizeof(ctime), "%Y-%m-%d", localtime(&sb->st_ctime));
    printf("ctime = %s\n", ctime);
    printf("start date = %s\n", start_date);

    // Check if the creation date of a file is larger than the start date input from client
    if (typeflag == FTW_F && strcmp(ctime, start_date) >= 0 ) {
        
        printf("file_path: %s\n", file_path);

        // Check if the file is existing in allFileNamesfda
        // If not, add its path and name into the allFileName
        if ( strstr(allFileNamesfda, file_path) == NULL ) {
            int a = combineFileNamefda (file_path, file_path + ftwbuf->base );
        }

        // Set this variable as 0 to indicate that searching is successful
        errorFLAGfda = 0;

        // return 0 to make the nftw() function continue the traverse
        return 0;
    }

    // If not existing, it returns 0 to continue the traverse
    else
        return 0;
}

// Create a TAR file that contains all the files founded in home directory, which is created after an start_date
int main ( int argc, char *argv[] ) {

    start_date = argv[1];
    // printf("date = %s\n", start_date);

    // char *home_dir = getenv("HOME");
    // Change the home directory later
    char *home_dir = "/home/song59/Desktop/asp/shellscript";

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkDateAfter, 20, FTW_PHYS);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAGfda == 0 ) {
            // printf("Search successful! All your requested files are showed above!\n\n");

            // Create the path of the TAR archive named temp.tar.gz in home directory
            char tar_filepath[PATH_MAX];
            sprintf(tar_filepath, "%s/temp.tar.gz", home_dir);

            // Construct the shell command to compress the files into a TAR archive using "tar -czf"
            char command[100000];   // a string to store the command
            int error = 0;
            sprintf (command, "tar -czf %s%s", tar_filepath, allFileNamesfda);

            // Execute the command using system()
            error = system(command);

            // If the TAR archive was created successfully, print successful message
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("\nTAR file created successful! The path is: \n%s\n\n", tar_filepath);
            }

            // Otherwise print a failure message
            else {
                printf("\nTAR file created unsuccessful!\n\n");
            }
        }

        // The value of errorFLAGfda will remain as -1 if there is no such file in the source directory
        else if ( errorFLAGfda == -1 ) {
            printf("\nNo file found\n\n");
        }
    }

    // nftw() returns -1 to searchResult when it detects an error and has not performed the traversal
    else if (searchResult == -1)
        printf("\nError Searching\n\n");

    return 1;
}
