#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <arpa/inet.h>

int main(int argc, char **argv);
int ispng(FILE *f);

int main(int argc, char **argv){
    U8 *f_crc_input = malloc(17);
    U8 *f_length = malloc(sizeof(U8)*4);
    U32 length = 0;
    U8 *f_crc = malloc(sizeof(U8) *4);
    U32 crc_val = 0;
    U8 *f_png_width = malloc(sizeof(U8) *4);
    U32 png_width = 0;
    U8 *f_png_height = malloc(sizeof(U8) *4);
    U32 png_height = 0;
    U32 crc_calc = 0;
    FILE *png;
    png = fopen(argv[1], "rb");
    if(png == NULL ){
        printf("%s: Not a PNG file\n",argv[1]);
    }
    else if(ispng(png) == 0){
        printf("%s: Not a PNG file\n",argv[1]);
    } else {
        fseek(png, 16, SEEK_CUR);
        fread(f_png_width, 1, 4, png);
        fread(f_png_height, 1, 4, png);

        memcpy(&png_width, f_png_width, sizeof(png_width));
        memcpy(&png_height, f_png_height, sizeof(png_height));
        png_width = (U32)ntohl(png_width);
        png_height = (U32)ntohl(png_height);
        
        
        fseek(png, 12, SEEK_SET);

        fread(f_crc_input, 1, 17, png);
        fread(f_crc, 1, 4, png);
        memcpy(&crc_val, f_crc, sizeof(crc_val));
        crc_val = (U32)ntohl(crc_val);

        crc_calc = crc(f_crc_input, 17);
        
        if(crc_calc != crc_val) {
            printf("%s: %u x %u\n", argv[1], png_width, png_height);
            printf("IDHL chunk CRC error: computed %x, expected %x\n", crc_calc, crc_val);
        } else {
            fread(f_length,1,4,png);
            memcpy(&length, f_length, sizeof(length));
            length = (U32)ntohl(length);

            f_crc_input = realloc(f_crc_input, 4+length);
            fread(f_crc_input, 4+length, 1, png);
            fread(f_crc, 4, 1, png);
            memcpy(&crc_val, f_crc, sizeof(crc_val));
            crc_val = (U32)ntohl(crc_val);

            crc_calc = crc(f_crc_input, 4+length);

            if(crc_calc != crc_val) {
                printf("%s: %u x %u\n", argv[1], png_width, png_height);
                printf("IDAT chunk CRC error: computed %x, expected %x\n", crc_calc, crc_val);
            } else {
                fseek(png, 4, SEEK_CUR);
                fread(f_crc_input, 4, 1, png);
                fread(f_crc, 4, 1, png);
                memcpy(&crc_val, f_crc, sizeof(crc_val));
                crc_val = (U32)ntohl(crc_val);

                crc_calc = crc(f_crc_input, 4);
                
                if(crc_calc != crc_val) {
                    printf("%s: %u x %u\n", argv[1], png_width, png_height);
                    printf("IEND chunk CRC error: computed %x, expected %x\n", crc_calc, crc_val);
                } else {
                    printf("%s: %u x %u\n", argv[1], png_width, png_height);
                }
            }
        }
    }
    
    free(f_crc_input);
    free(f_length);
    free(f_crc);
    free(f_png_width);
    free(f_png_height);
    fclose(png);
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

