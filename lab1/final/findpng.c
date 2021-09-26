#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <arpa/inet.h>
#include <dirent.h>


int main(int argc, char **argv);
void findpng(DIR *folder, int pngExists);
int ispng(FILE *f);

int main(int argc, char **argv) {
    DIR *folder;
    int pngExists = 0;
    folder = opendir(argv[1]);
    if(folder != NULL) {
        findpng(folder, pngExists);
    }

    if(pngExists == 0) {
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

void findpng(DIR *folder, int pngExists) {
    while(1) {
        struct dirent *entry;
        entry = readdir(folder);
        printf("%s\n", entry->d_name);
        entry = readdir(folder);
        printf("%s\n", entry->d_name);
        free(entry);
        break;
    }
}
