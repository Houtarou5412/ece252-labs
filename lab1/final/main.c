#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <arpa/inet.h>

/******************************************************************************
 * DEFINED MACROS 
 *****************************************************************************/
#define BUF_LEN  (256*16)
#define BUF_LEN2 (256*32)

/******************************************************************************
 * GLOBALS 
 *****************************************************************************/
U8 gp_buf_def[BUF_LEN2]; /* output buffer for mem_def() */
U8 gp_buf_inf[BUF_LEN2]; /* output buffer for mem_inf() */

//fucntion declarations
void pnginfo(char *filename);
void findpng();
void catpng();
void populate_IHDR_fields(FILE *png, U32 *IHDR_length, U32 *IHDR_type, U32 *IHDR_data_w, U32 *IHDR_data_h, U32 *IHDR_crc, U8 **IHDR_crc_input);
void populate_IDAT_fields(FILE *png, U32 *IDAT_length, U32 *IDAT_type, U8 *IDAT_data, U32 *IDAT_crc, U8 *IDAT_crc_input);
void populate_IEND_fields(FILE *png, U8 **IEND_type, U32 *IEND_crc, U32 *IDAT_length);

/******************************************************************************
 * FUNCTION PROTOTYPES 
 *****************************************************************************/
void init_data(U8 *buf, int len);
/**
 * @brief initialize memory with 256 chars 0 - 255 cyclically 
 */
void init_data(U8 *buf, int len)
{
    int i;
    for ( i = 0; i < len; i++) {
        buf[i] = i%256;
    }
}

void pnginfo(char *filename){
    U64 *header = malloc(8);  
    U32 *IHDR_length = malloc(4);
    U32 *IHDR_type = malloc(4);
    U32 *IHDR_data_w = malloc(4);
    U32 *IHDR_data_h = malloc(4);
    U8 *IHDR_data_d = malloc(1); //depth
    *IHDR_data_d = 8;
    U8 *IHDR_data_t = malloc(1); // type
    *IHDR_data_t = 6;
    U8 *IHDR_data_c = malloc(1); // compression
    *IHDR_data_c = 0;
    U8 *IHDR_data_f = malloc(1); // filter
    *IHDR_data_f = 0;
    U8 *IHDR_data_i = malloc(1); // interlace
    *IHDR_data_i = 0;
    U8 *IHDR_crc_input[17] = malloc(17);
    U32 *IHDR_crc = malloc(4);
    U32 *IHDR_crc2 = malloc(4);
    

    U32 *IDAT_length = malloc(4);
    U32 *IDAT_type = malloc(4);
    U8 *IDAT_data;
    U32 *IDAT_crc = malloc(4);
    U32 *IDAT_crc2 = malloc(4);
    U8 *IDAT_crc_input;

    U32 *IEND_length = malloc(4);;
    *IEND_length = 0;
    U8 *IEND_type[4] = malloc(1);
    U32 *IEND_crc = malloc(4);
    U32 *IEND_crc2 = malloc(4);


    FILE *png;
    png = fopen(filename, "rb");
    fread(header, 1,8, png);
    //*header = ntohl(*header);
    if(*header != 0x89504e470d0a1a0a){
        printf("%s: Not a PNG file\n", filename);
        return;
    }
    else{
        populate_IHDR_fields(png, IHDR_length, IHDR_type, IHDR_data_w, IHDR_data_h, IHDR_crc, IHDR_crc_input);
        populate_IDAT_fields(png, IDAT_length, IDAT_type, IDAT_data, IDAT_crc, IDAT_crc_input);
        populate_IEND_fields(png, IEND_type, IEND_crc, IDAT_length);
        *IHDR_crc2 = crc(*IHDR_crc_input, 17);
        *IDAT_crc2 = crc(IDAT_crc_input, 4+*IDAT_length);
        *IEND_crc2 = crc(*IEND_type, 4);

        char error_loc[4] = "";
        U32 c_crc;
        U32 e_crc;


        if(*IHDR_crc2 != *IHDR_crc){
            strcpy(error_loc, "IHDR");
            c_crc = *IHDR_crc2; 
            e_crc = *IHDR_crc;
        }
        else if(*IDAT_crc2 != *IDAT_crc){
            strcpy(error_loc, "IDAT");
            c_crc = *IDAT_crc2; 
            e_crc = *IDAT_crc;
        }
        else if(*IEND_crc2 != *IEND_crc){
            strcpy(error_loc, "IEND");
            c_crc = *IEND_crc2; 
            e_crc = *IEND_crc;
        }
        
        printf("%s: %u x %u\n", filename, *IHDR_data_w, *IHDR_data_h);
        if(strcmp(*error_loc,"") != 0){
            printf("%s chunk CRC error: computed %x, expected %x\n", error_loc, c_crc, e_crc);
        }
    }
}

void findpng(){

}

void catpng(){

}

void populate_IHDR_fields(FILE *png, U32 *IHDR_length, U32 *IHDR_type, U32 *IHDR_data_w, U32 *IHDR_data_h, U32 *IHDR_crc, U8 **IHDR_crc_input){
    rewind(png);
    fseek(png, 8, SEEK_CUR);
    fread(IHDR_length, 1, 4, png);
    //*IHDR_length = ntohl(*IHDR_length);
    fread(IHDR_type, 1, 4, png);
    //*IHDR_type = ntohl(*IHDR_type);
    fread(IHDR_data_w, 1, 4, png);
    //*IHDR_data_w = ntohl(*IHDR_data_w);
    fread(IHDR_data_h, 1, 4, png);
    //*IHDR_data_h = ntohl(*IHDR_data_h);

    fseek(png, 5, SEEK_CUR);
    fread(IHDR_crc, 1, 4, png);
    //*IHDR_crc = ntohl(*IHDR_crc);
    rewind(png);

    for(int i = 0; i < 12; i++){
        if(i < 4){
            *IHDR_crc_input[i] = *(IHDR_type + i);
        }
        else if(i < 8){
            *IHDR_crc_input[i] = *(IHDR_data_w + i-4);
        }
        else{
            *IHDR_crc_input[i] = *(IHDR_data_h + i-8);
        }
    }
    *IHDR_crc_input[12] = 8;
    *IHDR_crc_input[13] = 6;


}

void populate_IDAT_fields(FILE *png, U32 *IDAT_length, U32 *IDAT_type, U8 *IDAT_data, U32 *IDAT_crc, U8 *IDAT_crc_input){
    rewind(png);
    fseek(png, 33, SEEK_CUR);
    fread(IDAT_length, 1, 4, png);
    //*IDAT_length = ntohl(*IDAT_length);

    fread(IDAT_type, 1, 4, png);
    //*IDAT_type = ntohl(*IDAT_type);
    
    IDAT_data = malloc(*IDAT_length);
    fread(IDAT_data, 1, *IDAT_length, png);

    fread(IDAT_crc, 1, 4, png);
    //*IDAT_crc = ntohl(*IDAT_crc);

    for(int i = 0; i < *IDAT_length + 4; i++){
        if(i < 4){
            IDAT_crc_input[i] = IDAT_type[i];
        }
        else{
            IDAT_crc_input[i] = IDAT_data[i-4];
        }
    }

    rewind(png);
}

void populate_IEND_fields(FILE *png, U8 **IEND_type, U32 *IEND_crc, U32 *IDAT_length){
    rewind(png);
    fseek(png, 8+4+4+13+4+4+4+*IDAT_length+4+4, SEEK_CUR);
    fread(IEND_type,1,4, png);
    //*IEND_type = ntohl(*IEND_type);

    fread(IEND_crc,1,4, png);
    //*IEND_crc = ntohl(*IEND_crc);

    rewind(png);

}

int main(int argc, char **argv){
    
    if(strcmp(argv[0],"pnginfo") == 0){
        pnginfo(argv[1]);
    }
    else if(strcmp(argv[0],"findpng") == 0){
        findpng();
    }
    else if(strcmp(argv[0],"catpng") == 0){
        catpng();
    }

    return 0;
}