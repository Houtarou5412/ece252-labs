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
    remove("./all.png");
    //printf("1\n");
    int stop = 0;
    FILE **files = malloc(sizeof(FILE)*(argc-1));
    //int *order = (int*)malloc(sizeof(int)*(argc-1));
    U8 *headerlength = malloc(sizeof(U8)*12);
    U8 *IHDRtype = malloc(sizeof(U8)*4);
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
    U32 u_data_len = 0;
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

    if(ispng(files[0]) == 0) {
        printf("not png\n");
    }

    //printf("2\n");
    fread(headerlength, 12, 1, files[0]);
    fread(IHDRtype, 4, 1, files[0]);
    fread(width, 4, 1, files[0]);
    fseek(files[0], 4, SEEK_CUR);
    fread(after_height, 5, 1, files[0]);
    fseek(files[0], 4, SEEK_CUR);
    U8 *f_skip_len = malloc(sizeof(U8)*4);
    U32 skip_len = 0;
    fread(f_skip_len, 4, 1, files[0]);
    memcpy(&skip_len, f_skip_len, sizeof(skip_len));
    skip_len = (U32)ntohl(skip_len);
    //printf("skipped: %d\n", skip_len);
    fread(IDATtype, 4, 1, files[0]);
    fseek(files[0], skip_len+4, SEEK_CUR);
    fread(IEND, 12, 1, files[0]);

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

        //printf("%d x %lu\n", width_val, part_height);
        //printf("part length %d\n", part_length);
        temp_u_data = malloc(sizeof(U8)*(*part_u_data_length + u_data_len));
        for(unsigned long n = 0; n < u_data_len + *part_u_data_length; n++) {
            //printf("n: %u\n", n);
            if(u_data != NULL && n < u_data_len) {
                temp_u_data[n] = u_data[n];
            } else {
                temp_u_data[n] = part_u_data[n - u_data_len];
            }
            //printf("loop\n");
        }
        u_data_len += *part_u_data_length;
        height_val += part_height;
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

    //printf("5\n");
    IDATdata = malloc(sizeof(U8) * u_data_len);
    U32 *temp_size = malloc(sizeof(U32));
    mem_def(IDATdata, temp_size, u_data, u_data_len, -1);
    //printf("size of data: %d\n", *temp_size);
    memcpy(&total_length, temp_size, sizeof(total_length));
    free(temp_size);

    total_length = htonl(total_length);
    memcpy(IDATlength, &total_length, sizeof(total_length));
    total_length = (U32)ntohl(total_length);
    //printf("new height: %d\n", height_val);
    //printf("new length: %d\n", total_length);
    //total_length = (U32)htonl(total_length);
    height_val = (U32)htonl(height_val);
    memcpy(height, &height_val, sizeof(height_val));

    //printf("6\n");
    U32 temp_crc = 0;

    U8 *IHDRtypedata = malloc(sizeof(U8)*17);
    for(int o = 0; o < 17; o++) {
        //printf("o: %d\n", o);
        if(o < 4) {
            IHDRtypedata[o] = IHDRtype[o];
        } else if(o < 8) {
            IHDRtypedata[o] = width[o-4];
        } else if(o < 12) {
            IHDRtypedata[o] = height[o-8];
        } else {
            IHDRtypedata[o] = after_height[o-12];
        }
    }
    //printf("out\n");
    temp_crc = crc(IHDRtypedata, 17);
    //printf("bytes copied: %ld\n", sizeof(IHDRcrc));
    temp_crc = (U32)htonl(temp_crc);
    memcpy(IHDRcrc, &temp_crc, sizeof(IHDRcrc));
    //free(IHDRtypedata);

    //printf("6.5\n");
    U8 *IDATtypedata = malloc(4 + total_length);
    for(int p = 0; p < 4 + total_length; p++) {
        //printf("p: %d out of %d\n", p, total_length);
        if(p < 4) {
            IDATtypedata[p] = IDATtype[p];
        } else {
            IDATtypedata[p] = IDATdata[p-4];
        }
    }
    temp_crc = crc(IDATtypedata, 4 + total_length);
    temp_crc = (U32)htonl(temp_crc);
    //printf("6.75\n");
    memcpy(IDATcrc, &temp_crc, sizeof(IDATcrc));

    //free(IDATtypedata);

    //printf("7\n");
    char outname[10] = "./all.png";
    FILE *outfile = fopen(outname, "wb+");
    if(outfile == NULL) {
        printf("null\n");
    }
    //printf("7.5\n");
    /*for(int g = 0; g < sizeof(headerlength); g++) {
        fprintf(outfile, "%c", headerlength[g]);
    }*/
    fwrite(headerlength, 1, 12, outfile);
    //fflush(outfile);
    //printf("headerlength: %s\n", headerlength);
    fwrite(IHDRtypedata, 1, 17, outfile);
    //fflush(outfile);
    //printf("IHDRtypedata %s\n", IHDRtypedata);
    fwrite(IHDRcrc, 1, 4, outfile);
    //fflush(outfile);
    //printf("IHDRcrc done\n");
    fwrite(IDATlength, 1, 4, outfile);
    //fflush(outfile);
    //printf("IDATlength done\n");
    fwrite(IDATtypedata, 1, 4+total_length, outfile);
    //fflush(outfile);
    //printf("IDATtypedata done\n");
    fwrite(IDATcrc, 1, 4, outfile);
    //fflush(outfile);
    //printf("IDATcrc done\n");
    fwrite(IEND, 1, 12, outfile);
    //fflush(outfile);
    //printf("IEND done\n");

    //printf("8\n");
    fclose(outfile);
    for(int t = 0; t < argc-1; t++) {
        fclose(files[t]);
    }
    //free(order);
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
    free(headerlength);
    free(IHDRtype);

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