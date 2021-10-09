#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <string.h>
#include <pthread.h>

#define IMG_URL "http://ece252-1.uwaterloo.ca:2520/image?img=1"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    U8 *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

//GLOBALS
RECV_BUF * recv_buf;
int success;
int crops;
pthread_mutex_t synch;

//FUNCTION DECLARATIONS
int main(int argc, char **argv);
int ispng(FILE *f);
int catpng(int argc, RECV_BUF * recvbuf);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);

//FUNCTION DEFINITIONS
/**
 * @brief  cURL header call back function to extract image sequence number from 
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line 
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv, 
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	    return 1;
    }
    
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

void get_strips(char *img_url) {
    CURL *curl_handle;
    CURLcode res;
    curl_handle = curl_easy_init();

    /*if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return 1;
    }*/
    char *thread_url;

    pthread_mutex_lock(&synch);
    strcpy(thread_url, img_url);
    while(success < crops) {
        pthread_mutex_unlock(&synch);
        RECV_BUF temp_buf;
        recv_buf_init(&temp_buf, BUF_SIZE);

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, thread_url);

        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&temp_buf);

        /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&temp_buf);

        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        
        /* get it! */
        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            //printf("%lu bytes received in memory %p, seq=%d, using url %s.\n", \
                temp_buf.size, temp_buf.buf, temp_buf.seq, img_url);
        }

        //temp
        //temp_buf.seq = 0;

        pthread_mutex_lock(&synch);
        if(recv_buf[temp_buf.seq].size <= 0) {
            //printf("made it\n");
            recv_buf[temp_buf.seq].size = temp_buf.size;
            recv_buf[temp_buf.seq].buf = temp_buf.buf;
            // memcpy(recv_buf[temp_buf.seq].buf, temp_buf.buf, temp_buf.size);
            recv_buf[temp_buf.seq].seq = temp_buf.seq;
            //printf("finished\n");

            success++;
            //printf("Successfully added section %d\n", temp_buf.seq);
        } else {
            recv_buf_cleanup(&temp_buf);
        }

        /*if(img_url[14] == '3') {
            img_url[14] = '1';
        } else {
            img_url[14] = img_url[14] + 1;
        }*/
    }
    pthread_mutex_unlock(&synch);
    curl_easy_cleanup(curl_handle);
}

int main(int argc, char **argv) {
    printf("something\n");
    //Config
    crops = 50;

    char img_url[] = "http://ece252-1.uwaterloo.ca:2520/image?img=1";
    int threads = 1;

    //Getting command options
    for(int t = 0; t < argc; t++) {
        printf("at arg: %d\n", t);
        if(strcmp(argv[0],"-t") == 0) {
            printf("assigning threads\n");
            threads = atoi(argv[t+1]);
        } else if(strcmp(argv[0],"-n") == 0) {
            printf("assigning image\n");
            img_url[strlen(img_url)-2] = argv[t+1][0];
        }
    }

    //cURL
    recv_buf = malloc(sizeof(RECV_BUF) * crops);

    memset(recv_buf, 0, sizeof(RECV_BUF) * crops);
    char fname[256];
    pid_t pid =getpid();
    
    //Initialize recv_buf array
    // for(int i = 0; i < crops; i++) {
    //     recv_buf_init(&(recv_buf[i]), BUF_SIZE);
    //     printf("recv_buf[%d] located at %p\n", i, &(recv_buf[i]));
    // }
    
    printf("%s: URL is %s\n", argv[0], img_url);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    success = 0;

    //MUTEX INIT
    pthread_mutex_init(&synch, NULL);

    //THREADS HERE
    pthread_t *p_tids = malloc(sizeof(pthread_t) * threads);
    struct thread_ret *p_results[threads];
     
    for (int i=0; i<threads; i++) {
        printf("Creating thread: %d", i);

        pthread_mutex_lock(&synch);
        pthread_create(p_tids + i, NULL, get_strips, img_url);
        pthread_mutex_unlock(&synch);

        if(img_url[14] == '3') {
            img_url[14] = '1';
        } else {
            img_url[14] = img_url[14] + 1;
        }
    }

    for (int i=0; i<threads; i++) {
        pthread_join(p_tids[i], (void **)&(p_results[i]));
        //printf("Thread ID %lu joined.\n", p_tids[i]);
        //printf("sum(%d,%d) = %d.\n", \
               in_params[i].x, in_params[i].y, p_results[i]->sum); 
        //printf("product(%d,%d) = %d.\n\n", \
               in_params[i].x, in_params[i].y, p_results[i]->product); 
    }

    /* cleaning up */

    free(p_tids);

    printf("Completed all 50 sections\n");

    /*for(int m = 0; m < crops; m++) {
        printf("Verify %lu bytes received in memory %p, seq=%d.\n", recv_buf[m].size, recv_buf[m].buf, recv_buf[m].seq);
    }*/

    //sprintf(fname, "./output_%d_%d.png", recv_buf.seq, pid);
    //write_file(fname, recv_buf.buf, recv_buf.size);

    /*(char name[] = "pic0.png";
    FILE *f = fopen(name, "wb+");
    fwrite(recv_buf[0].buf, 1, recv_buf[0].size, f);
    fclose(f);*/

    catpng(crops, recv_buf);

    /* cleaning up */
    curl_global_cleanup();
    for(int j = 0; j < crops; j++) {
        recv_buf_cleanup(&(recv_buf[j]));
    }
    free(recv_buf);
    pthread_mutex_destroy(&synch);

    return 0;
}

int catpng(int argc, RECV_BUF * recv_buf) {
    remove("./all.png");
    printf("1\n");
    int stop = 0;
    //FILE **files = malloc(sizeof(FILE)*(argc-1));
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
    /*for(int i = 1; i < argc; i++) {
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
    }*/

    printf("2\n");

    //Collecting variables for files
    /*fread(headerlength, 12, 1, files[0]);
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
    width_val = (U32)ntohl(width_val);*/

    //Collecting variables for recv_buf array
    memcpy(headerlength, recv_buf[0].buf, sizeof(U8)*12);
    memcpy(IHDRtype, recv_buf[0].buf + 12, sizeof(U8)*4);
    memcpy(width, recv_buf[0].buf + 12 + 4, sizeof(U8)*4);
    memcpy(after_height, recv_buf[0].buf + 12 + 4 + 4 + 4, sizeof(U8)*5);
    U8 *f_skip_len = malloc(sizeof(U8)*4);
    U32 skip_len = 0;
    memcpy(f_skip_len, recv_buf[0].buf + 12 + 4 + 4 + 4 + 5 + 4, sizeof(U8)*4);
    memcpy(&skip_len, f_skip_len, sizeof(U8)*4);
    skip_len = (U32)ntohl(skip_len);
    printf("skipped: %d\n", skip_len);
    memcpy(IDATtype, recv_buf[0].buf + 12 + 4 + 4 + 4 + 5 + 4 + 4, sizeof(U8)*4);
    memcpy(IEND, recv_buf[0].buf + 12 + 4 + 4 + 4 + 5 + 4 + 4 + 4 + skip_len + 4, sizeof(U8)*12);

    free(f_skip_len);
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
    for(int m = 0; m < argc; m++) {
        printf("3\n");
        U8 *height = malloc(sizeof(U8)*4);
        U64 part_height = 0;
        U8 *length = malloc(sizeof(U8)*4);
        U64 part_length = 0;
        U8 *data = NULL;
        U8 *part_u_data = NULL;
        U8 *temp_u_data = NULL;
        U64 part_u_data_length = 0;

        //FILES
        /*fseek(files[m], 20, SEEK_CUR);
        fread(height, 4, 1, files[m]);
        memcpy(&part_height, height, sizeof(part_height));
        part_height = (U64)ntohl(part_height);
        fseek(files[m], 9, SEEK_CUR);
        fread(length, 4, 1, files[m]);
        memcpy(&part_length, length, sizeof(part_length));
        part_length = (U64)ntohl(part_length);

        fseek(files[m], 4, SEEK_CUR);
        data = malloc(sizeof(U8)*part_length);
        fread(data, part_length, 1, files[m]);*/

        //RECV_BUF
        memcpy(height, recv_buf[m].buf + 20, sizeof(U8)*4);
        memcpy(&part_height, height, sizeof(part_height));
        part_height = (U64)ntohl(part_height);
        memcpy(length, recv_buf[m].buf + 20 + 4 + 9, sizeof(U8)*4);
        memcpy(&part_length, length, sizeof(part_length));
        part_length = (U64)ntohl(part_length);

        data = malloc(sizeof(U8)*part_length);
        memcpy(data, recv_buf[m].buf + 20 + 4 + 9 + 4 + 4, part_length);

        part_u_data = malloc(sizeof(U8)*part_height*(width_val*4 + 1));
        memset(part_u_data, 0, sizeof(U8)*part_height*(width_val*4 + 1));
        mem_inf(part_u_data, &part_u_data_length, data, part_length);

        //test
        /*U8 *test_data = malloc(part_u_data_length);
        memset(test_data, 0, part_u_data_length);
        U64 test_size = 0;
        mem_def(test_data, &test_size, part_u_data, part_u_data_length, Z_DEFAULT_COMPRESSION);
        // for(int n = 0; n < part_length; n++) {
        //     if(data[n] != test_data[n]) {
        //         printf("n: %d\n", n);
        //     }
        // }
        printf("test_size: %lu\n",test_size);
        free(test_data);*/

        printf("%d x %lu\n", width_val, part_height);
        printf("part length %d\n", part_length);
        printf("part u data length %d\n", part_u_data_length);
        temp_u_data = malloc(sizeof(U8)*(part_u_data_length + u_data_len));
        int test = 0;
        for(unsigned long n = 0; n < u_data_len + part_u_data_length; n++) {
            //printf("n: %u\n", n);
            if(u_data != NULL && n < u_data_len) {
                temp_u_data[n] = u_data[n];
            } else {
                temp_u_data[n] = part_u_data[n - u_data_len];
                test++;
            }
            //printf("loop %d\n", n);
        }
        printf("test: %d\n", test);

        u_data_len += part_u_data_length;
        height_val += part_height;
        printf("4.5\n");
        free(u_data);
        u_data = temp_u_data;
        temp_u_data = NULL;

        free(height);
        free(length);
        free(data);
        free(part_u_data);
    }

    printf("5\n");
    IDATdata = malloc(sizeof(U8) * u_data_len);
    U64 *temp_size = malloc(sizeof(U64));
    mem_def(IDATdata, temp_size, u_data, u_data_len, -1);
    //printf("size of data: %d\n", *temp_size);
    memcpy(&total_length, temp_size, sizeof(total_length));

    total_length = htonl(total_length);
    memcpy(IDATlength, &total_length, sizeof(total_length));
    total_length = (U32)ntohl(total_length);
    printf("new height: %d\n", height_val);
    printf("new length: %d\n", *temp_size);
    printf("u data len: %d\n", u_data_len);
    //total_length = (U32)htonl(total_length);
    height_val = (U32)htonl(height_val);
    memcpy(height, &height_val, sizeof(height_val));

    free(temp_size);

    printf("6\n");
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
    memcpy(IHDRcrc, &temp_crc, sizeof(U8)*4);
    //free(IHDRtypedata);

    printf("6.5\n");
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
    printf("6.75\n");
    memcpy(IDATcrc, &temp_crc, sizeof(U8)*4);

    //free(IDATtypedata);

    printf("7\n");
    char outname[10] = "./all.png";
    FILE *outfile = fopen(outname, "wb+");
    if(outfile == NULL) {
        printf("null\n");
    }
    printf("7.5\n");
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

    printf("8\n");
    fclose(outfile);
    /*for(int t = 0; t < argc-1; t++) {
        fclose(files[t]);
    }*/
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