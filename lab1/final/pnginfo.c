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
    U8 *f_IHDR_crc_input = malloc(17);
    U8 *f_IHDR_crc = malloc(sizeof(U8) *4);
    U8 *f_png_width = malloc(sizeof(U8) *4);
    U32 png_width = 0;
    U8 *f_png_height = malloc(sizeof(U8) *4);
    U32 png_height = 0;
    U32 crc_calc = 0;
    int crc_match = 1;
    FILE *png;
    png = fopen(argv[1], "rb");
    if(png == NULL || ispng(png) == 0){
        printf("%s: Not a PNG file\n",argv[1]);
    }
    else{
        fseek(png, 8, SEEK_CUR);
        fread(f_IHDR_length, 1, 4, png);

        fseek(png, 4, SEEK_CUR);
        fread(f_png_width, 1, 4, png);
        fread(f_png_height, 1, 4, png);
        
        fseek(png, 12, SEEK_SET);

        fread(f_IHDR_crc_input, 1, 17, png);
        fread(f_IHDR_crc, 1, 4, png);

        crc_calc = crc(f_IHDR_crc_input, 17);
        for(int i = 0; i < 4; i++) {
            if(((crc_calc >> (8*i)) & 0xff) != f_IHDR_crc[i]) {
                crc_match = 0;
            }
        }
        

        //*(&crc_calc + 0x2)
        
        
    }
    free(IHDR_length);
    free(IHDR_crc_input);
    free(IHDR_crc);
    free(png_width_string);
    free(png_height_string);
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

