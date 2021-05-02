#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main (int argc, char *argv[])  {

    DIR* FD;
    int results;
    FILE * fp;
    struct dirent *macFile, *opFile;
    char *fName, *opName;
    char dName[16] = "./public_tests/";
    if (NULL == (FD = opendir("./public_tests/"))) {
        printf("\n\nCould not open public tests directory\n\n");
    } else {
        
        while ((macFile = readdir(FD))) {
            if(strlen(macFile -> d_name) <= 2) {
                continue;
            }
            readdir(FD);
            opFile = readdir(FD);
            fName = malloc(16 + strlen(macFile->d_name));
            strcpy(fName, dName);
            strcpy(fName + 15, macFile->d_name);

            opName = malloc(16 + strlen(opFile->d_name));
            strcpy(opName, dName);
            strcpy(opName + 15, opFile->d_name);

            results = open("op.txt", O_WRONLY | O_TRUNC, 0666);
            if (fork() == 0) {
                close(1);
                dup(results);
                execl("./sim-pipe","./sim-pipe",fName,NULL);
            } else {
                close(results);
                wait(NULL);
            }
            free(fName);

            results = open("results.txt", O_WRONLY | O_TRUNC, 0666);
            if (fork() == 0) {
                close(1);
                dup(results);
                execl("/bin/diff", "/bin/diff", "op.txt", opName, NULL);
            } else {
                close(results);
                wait(NULL);
            }

            fp = fopen("results.txt", "r");
            fseek(fp, 0, SEEK_END);
            if (ftell(fp) == 0) {
                printf("%s Passed!\n", opName);
            } else {
                printf("%s Failed :(\n", opName);
                break;
            }
            free(opName);
        }
        closedir(FD);
        
    }
    return 0;
}