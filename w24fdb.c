#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <time.h>
#include <sys/wait.h>

char *end_date;
int errorFLAG = -1;             // this flag is for printing appropriate error messages when searching for files as request
char allFileNames[100000];      // store all the file paths and names for tar command


// Create a formatted string that represents the file paths and names for archiving
// The shell command format for filepath: -C "filepath1" filename1.extension -C "filepath2" filename2.extension
// Using this format with "-C" is for only archiving the file instead of including all their directories
int combineFileName ( const char *filepath, const char *filename ) {
    char quoted_path[PATH_MAX + 3];     // Store quoted directory

    // Extract the directory path of the current filepath
    char *file_dir = dirname (filepath);

    // This format is to archive all the files without their directory structure
    sprintf(allFileNames, "%s -C \"%s\" \"%s\"", allFileNames, file_dir, filename);

    // Return 1 to indicate successful completion
    return 1;
}

int checkDate ( const char *filepath,
                  const struct stat *sb,
                  int typeflag,
                  struct FTW *ftwbuf) {

    char ctime[10];
    strftime(ctime, sizeof(ctime), "%Y-%m-%d", localtime(&sb->st_ctime));

    // Check if the creation date of a file is as request
    if (typeflag == FTW_F && strcmp(ctime, end_date) <= 0 ) {
        
        printf("%s: %s\n", filepath + ftwbuf->base, ctime);

        // Check if the file is existing in allFileNames
        // If not, add its path and name into the allFileName
        if ( strstr(allFileNames, filepath) == NULL ) {
            int a = combineFileName (filepath, filepath + ftwbuf->base );
        }

        // Set this variable as 0 to indicate that searching is successful
        errorFLAG = 0;

        // return 0 to make the nftw() function continue the traverse
        return 0;
    }

    // If not existing, it returns 0 to continue the traverse
    else
        return 0;
}

// Create a TAR file that contains all the files founded in home directory, which is created before an end_date
int main ( int argc, char *argv[] ) {

    end_date = argv[1];
    // printf("date = %s\n", end_date);

    // char *home_dir = getenv("HOME");
    // Change the home directory later
    char *home_dir = "/Users/nanasmacbookprowithtouchbar/folder1";

    // Traverse the home directory
    int searchResult = nftw(home_dir, checkDate, 20, FTW_PHYS);

    // Search successful with no errors during traversal
    if ( searchResult == 0 ){

        // All files were found successfully
        if ( errorFLAG == 0 ) {
            // printf("Search successful! All your requested files are showed above!\n\n");

            // Create the path of the TAR archive named temp.tar.gz in home directory
            char tar_filepath[PATH_MAX];
            sprintf(tar_filepath, "%s/temp.tar.gz", home_dir);

            // Construct the shell command to compress the files into a TAR archive using "tar -czf"
            char command[100000];   // a string to store the command
            int error = 0;
            sprintf (command, "tar -czf %s%s", tar_filepath, allFileNames);

            // Execute the command using system()
            error = system(command);

            // If the TAR archive was created successfully, print successful message
            if ( WIFEXITED(error) && WEXITSTATUS(error) == 0 ) {
                printf("\nTAR file created successful! The path is: \n%s\n\n", tar_filepath);
            }

            // Otherwise print a failure message
            else
                printf("\nTAR file created unsuccessful!\n\n");
        }

        // The value of errorFLAG will remain as -1 if there is no such file in the source directory
        else if ( errorFLAG == -1 ) {
            printf("\nNo file found\n\n");
        }
    }

    // nftw() returns -1 to searchResult when it detects an error and has not performed the traversal
    else if (searchResult == -1)
        printf("\nError Searching\n\n");

    return 1;
}
