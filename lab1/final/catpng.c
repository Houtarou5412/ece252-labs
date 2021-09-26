#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <arpa/inet.h>
#include <dirent.h>


int main(int argc, char **argv);
int ispng(FILE *f);

int main(int argc, char **argv) {
    int stop = 0;
    FILE **files = malloc(sizeof(FILE)*(argc-1));
    int *order = (int*)malloc(sizeof(int)*(argc-1));
    for(int i = 1; i < argc; i++) {
        printf("%d\n", i);
        for(int j = 1; j < argc; j++) {
            printf("with %d @ char %c vs %c\n", j, argv[j][strlen(argv[j])-5], itoa(i)[0]);
            if(argv[j][strlen(argv[j])-5] == itoa(i)[0]) {
                printf("true\n");
                files[i-1] = fopen(argv[j], "rb");
                order[i-1] = j;
            }
        }
    }

    for(int s = 0; s < argc-1; s++) {
        printf("%s\n", argv[order[s]]);
    }

    for(int t = 0; t < argc-1; t++) {
        fclose(files[t]);
    }
    free(order);
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