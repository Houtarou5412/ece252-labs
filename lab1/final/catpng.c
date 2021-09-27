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
    printf("1\n");
    int stop = 0;
    FILE **files = malloc(sizeof(FILE)*(argc-1));
    //int *order = (int*)malloc(sizeof(int)*(argc-1));
    U8 *before_width = malloc(sizeof(U8)*16);
    U8 *width = malloc(sizeof(U8)*4);
    U32 width_val = 0;
    U8 *height = malloc(sizeof(U8)*4);
    U32 height_val = 0;
    U8 *after_height = malloc(sizeof(U8)*5);
    U8 *IHDRcrc = malloc(sizeof(U8)*4);
    //memset(IHDRcrc, 0, sizeof(U8)*4);
    //printf("bytes copied: %d\n", sizeof(IHDRcrc));
    U8 *IDATlength = malloc(sizeof(U8)*4);
    U32 total_length = 0;
    U8 *IDATtype = malloc(sizeof(U8)*4);
    U8 *u_data = NULL;
    U8 *IDATdata = NULL;
    U8 *IDATcrc = malloc(sizeof(U8)*4);
    U8 *IEND = malloc(sizeof(U8)*12);
    for(int i = 1; i < argc; i++) {
        //printf("%d\n", i);
        for(int j = 1; j < argc; j++) {
            //printf("with %d @ char %c vs %c\n", j, argv[j][strlen(argv[j])-5], (char)(i+47));
            if(argv[j][strlen(argv[j])-5] == (char)(i+47)) {
                //printf("true\n");
                files[i-1] = fopen(argv[j], "rb");
                //order[i-1] = j;
            }
        }
    }
    printf("2\n");
    fread(before_width, 16, 1, files[0]);
    fread(width, 4, 1, files[0]);
    fseek(files[0], 4, SEEK_CUR);
    fread(after_height, 5, 1, files[0]);
    fseek(files[0], 4, SEEK_CUR);
    U8 *f_skip_len = malloc(sizeof(U8)*4);
    U32 skip_len = 0;
    fread(f_skip_len, 4, 0, files[0]);
    memcpy(&skip_len, f_skip_len, sizeof(skip_len));
    skip_len = (U32)ntohl(skip_len);
    fread(IDATtype, 4, 0, files[0]);
    fseek(files[0], skip_len+4, SEEK_CUR);
    fread(IEND, 12, 0, files[0]);

    free(f_skip_len);
    rewind(files[0]);
    memcpy(&width_val, width, sizeof(width_val));
    width_val = (U32)ntohl(width_val);
    /*for(int s = 0; s < argc-1; s++) {
        printf("%s\n", argv[order[s]]);
    }*/
    
    // For each file
    // get height and IDAT length
    // get IDAT data
    // uncompress
    // concatenate w/ existing data
    //
    // end loop, compress, write to file
    for(int m = 0; m < argc-1; m++) {
        //printf("3\n");
        U8 *height = malloc(sizeof(U8)*4);
        U64 part_height = 0;
        U8 *length = malloc(sizeof(U8)*4);
        U64 part_length = 0;
        U8 *data = NULL;
        U8 *part_u_data = NULL;
        U8 *temp_u_data = NULL;
        U64 *part_u_data_length = malloc(sizeof(U64));

        fseek(files[m], 20, SEEK_CUR);
        fread(height, 4, 1, files[m]);
        memcpy(&part_height, height, sizeof(part_height));
        part_height = (U64)ntohl(part_height);
        fseek(files[m], 9, SEEK_CUR);
        fread(length, 4, 1, files[m]);
        memcpy(&part_length, length, sizeof(part_length));
        part_length = (U64)ntohl(part_length);

        fseek(files[m], 4, SEEK_CUR);
        data = malloc(sizeof(U8)*part_length);
        fread(data, part_length, 1, files[m]);

        part_u_data = malloc(sizeof(U8)*part_height*(width_val*4 + 1));
        mem_inf(part_u_data, part_u_data_length, data, part_length);

        //printf("4\n");
        temp_u_data = malloc(sizeof(U8)*(*part_u_data_length + sizeof(u_data)));
        for(unsigned long n = 0; n < sizeof(u_data) + *part_u_data_length; n++) {
            //printf("n: %u\n", n);
            if(u_data != NULL && n < sizeof(u_data)) {
                temp_u_data[n] = u_data[n];
            } else {
                temp_u_data[n] = part_u_data[n - sizeof(u_data)];
            }
            //printf("loop\n");
        }
        //printf("4.5\n");
        free(u_data);
        u_data = temp_u_data;
        temp_u_data = NULL;

        free(height);
        free(length);
        free(data);
        free(part_u_data);
        free(part_u_data_length);
    }

    printf("5\n");
    IDATdata = malloc(sizeof(U8)*sizeof(u_data));
    U32 *temp_size = malloc(sizeof(U64));
    mem_def(IDATdata, temp_size, u_data, sizeof(u_data), -1);
    printf("size of data: %d", *temp_size);
    memcpy(IDATlength, temp_size, sizeof(IDATlength));
    free(temp_size);

    memcpy(&total_length, IDATlength, sizeof(total_length));
    total_length = (U32)ntohl(total_length);
    height_val = total_length/(width_val*4 + 1);
    total_length = (U32)htonl(total_length);
    height_val = (U32)htonl(height_val);
    memcpy(height, &height_val, sizeof(height_val));

    printf("6\n");
    U32 temp_crc = 0;

    U8 *IHDRtypedata = malloc(sizeof(U8)*17);
    for(int o = 0; o < 17; o++) {
        printf("o: %d\n", o);
        if(o < 4) {
            IHDRtypedata[o] = before_width[o+12];
        } else if(o < 8) {
            IHDRtypedata[o] = width[o-4];
        } else if(o < 12) {
            IHDRtypedata[o] = height[o-8];
        } else {
            IHDRtypedata[o] = after_height[o-12];
        }
    }
    printf("out\n");
    temp_crc = crc(IHDRtypedata, 17);
    printf("bytes copied: %ld\n", sizeof(IHDRcrc));
    temp_crc = (U32)htonl(temp_crc);
    memcpy(IHDRcrc, &temp_crc, sizeof(IHDRcrc));
    //free(IHDRtypedata);

    printf("6.5\n");
    U8 *IDATtypedata = malloc(4 + total_length);
    for(int p = 0; p < 4 + total_length; p++) {
        printf("p: %d out of %d\n", p, total_length);
        if(p < 4) {
            IDATtypedata[p] = IDATtype[p];
        } else {
            IDATtypedata[p] = IDATdata[p-4];
        }
    }
    temp_crc = crc(IDATtypedata, 4 + total_length);
    temp_crc = (U32)htonl(temp_crc);
    printf("6.75\n");
    memcpy(IDATcrc, &temp_crc, sizeof(IDATcrc));

    //free(IDATtypedata);

    printf("7\n");
    char *outname = "./all.png";
    FILE *outfile = fopen(outname, "rb+");
    fprintf(outfile, "%s", before_width);
    fprintf(outfile, "%s", IHDRtypedata);
    fprintf(outfile, "%s", IHDRcrc);
    fprintf(outfile, "%s", IDATlength);
    fprintf(outfile, "%s", IDATtypedata);
    fprintf(outfile, "%s", IDATcrc);
    fprintf(outfile, "%s", IEND);

    printf("8\n");
    fclose(outfile);
    for(int t = 0; t < argc-1; t++) {
        fclose(files[t]);
    }
    //free(order);
    free(before_width);
    free(width);
    free(height);
    free(after_height);
    free(IHDRcrc);
    free(IDATlength);
    free(IDATtype);
    free(IDATdata);
    free(IDATcrc);
    free(IEND);
    free(u_data);
    free(IHDRtypedata);
    free(IDATtypedata);

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