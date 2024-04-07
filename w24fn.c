#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <time.h>

const char *searched_filename = "b1.txt";  // store the name of the input file
const char *searched_filepath;
char permission_message[13];

// nftw() callback function to look for the first occurrence of the input file
int checkFirst ( const char *filepath,
                 const struct stat *sb,
                 int typeflag,
                 struct FTW *ftwbuf ) {

    // Check if the input file existing in the traversed path
    if (typeflag == FTW_F && strstr(filepath, searched_filename) != NULL) {

        searched_filepath = filepath;

        printf("File: %s\n", filepath);
        printf("Size: %ld bytes\n", sb->st_size);
        printf("Date of Creation: %s\n", ctime(&sb->st_ctime));
        snprintf(permission_message, sizeof(permission_message),
                 "%c%c%c%c%c%c%c%c%c%c\n",
                 (S_ISDIR(sb->st_mode)) ? 'd' : '-',
                 (sb->st_mode & S_IRUSR) ? 'r' : '-',
                 (sb->st_mode & S_IWUSR) ? 'w' : '-',
                 (sb->st_mode & S_IXUSR) ? 'x' : '-',
                 (sb->st_mode & S_IRGRP) ? 'r' : '-',
                 (sb->st_mode & S_IWGRP) ? 'w' : '-',
                 (sb->st_mode & S_IXGRP) ? 'x' : '-',
                 (sb->st_mode & S_IROTH) ? 'r' : '-',
                 (sb->st_mode & S_IWOTH) ? 'w' : '-',
                 (sb->st_mode & S_IXOTH) ? 'x' : '-');

        // return 1 to make the nftw() function stop the traverse after the first occurrence
        return 1;
    }
    else
        return 0;
}

int main() {

    const char *home_dir = getenv("HOME");

    // Traverse all files in the home directory, the second argument
    int searchResult = nftw(home_dir, checkFirst, 20, FTW_NS);

    // nftw() returns 1 if the callback function checkFirst() returns 1
    // Which means the file has been found
    if ( searchResult > 0 ){
        printf("Permission: %s\n", permission_message);
    }

        // checkFirst() function returns 0 when the tree is exhausted
        // Which means the traversal was performed but the file was not found
        // In this case, nftw() also returns 0 to searchResult
    else {
        printf("\nFile not found!\n\n");
    }

    return 0;
}