#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <arpa/inet.h>
#include <dirent.h>


int main(int argc, char **argv);
void findpng(DIR *folder, int *pngExists, char path[]);
int ispng(FILE *f);

int main(int argc, char **argv) {
    printf("arg: %s\n", argv[1]);
    DIR *folder = NULL;
    int *pngExists = malloc(sizeof(int));
    *pngExists = 0;
    char path[1000];
    for(int i = 0; i < strlen(argv[1]); i++) {
        path[i] = argv[1][0];
    }
    folder = opendir(argv[1]);
    printf("main path: %s\n", path);
    if(folder != NULL) {
        printf("2\n");
        findpng(folder, pngExists, path);
        closedir(folder);
    }

    if(*pngExists == 0) {
        printf("findpng: No PNG file found\n");
    }
    free(pngExists);
    return 0;
}

int ispng(FILE *f){
    U8 *header = malloc(sizeof(U8)*8);
    int pngCode[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    rewind(f);
    fread(header, 8, 1, f);
    rewind(f);
    for(int i = 0; i < 8; i++) {
        if(header[i] != pngCode[i]) {
            free(header);
            return 0;
        }
    }
    free(header);
    return 1;
}

void findpng(DIR *folder, int *pngExists, char path[]) {
    struct dirent *entry;
    printf("3\n");
    while(1) {
        entry = readdir(folder);
        if(entry == NULL) {
            break;
        }
        printf("4\n");
        if(entry->d_type == DT_DIR && strcmp(entry->d_name,".") != 0 && strcmp(entry->d_name,"..") != 0) {
            int j = 0;
            for(int i = 0; j < 1+strlen(entry->d_name); i++) {
                printf("5\n");
                if(path[i] == '\0' && j == 0) {
                    path[i] = '/';
                    j++;
                } else if(path[i] == '\0') {
                    path[i] == entry->d_name[j-1];
                    j++;
                }
            }
            printf("6\n");
            DIR *subfolder = opendir(entry->d_name);
            findpng(subfolder, pngExists, path);
            closedir(subfolder);
        } else if(entry->d_type == DT_REG) {
            printf("7\n");
            char filepath[1000];
            int j = 0;
            for(int i = 0; j < 1+strlen(entry->d_name); i++) {
                printf("8\n");
                if(path[i] != '\0') {
                    filepath[i] = path[i];
                } else if(j == 0) {
                    filepath[i] = '/';
                    j++;
                } else {
                    filepath[i] = entry->d_name[j-1];
                    j++;
                }
            }
            printf("path is %s\n", filepath);
            FILE *file = fopen(filepath, "rb");
            printf("%s\n", entry->d_name);
            if(file != NULL && ispng(file)) {
                printf("%s", filepath);
                *pngExists = 1;
            }
            printf("9\n");
            if(file != NULL) {
                fclose(file);
            }
        }
    }
    entry = NULL;
}
