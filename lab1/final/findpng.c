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
    DIR *folder;
    int *pngExists = malloc(sizeof(int));
    *pngExists = 0;
    char path[1000000000];
    folder = opendir(argv[1]);
    if(folder != NULL) {
        findpng(folder, pngExists, path);
    }

    if(*pngExists == 0) {
        printf("findpng: No PNG file found\n");
    }
    closedir(folder);
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
    while(1) {
        entry = readdir(folder);
        if(entry == NULL) {
            break;
        }
        if(entry->d_type == DT_DIR && strcmp(entry->d_name,".") != 0 && strcmp(entry->d_name,"..") != 0) {
            int j = 0;
            for(int i = 0; j < 1+strlen(entry->d_name); i++) {
                if(path[i] == '' && j == 0) {
                    path[i] = '/';
                    j++;
                } else if(path[i] == '') {
                    path[i] == entry->d_name[j-1];
                    j++;
                }
            }
            DIR *subfolder = opendir(entry->d_name);
            findpng(subfolder, pngExists, path);
            closedir(subfolder);
        } else if(entry->d_type == DT_REG) {
            FILE *file = fopen(entry->d_name, "rb");
            if(ispng(file)) {
                printf("%s/%s", path, entry->d_name);
                *pngExists = 1;
            }
            fclose(file);
        }
    }
    entry = NULL;
}