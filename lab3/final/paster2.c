#define _DEFAULT_SOURCE
#define STRIP_NUM 50

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "cURL.h"
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include <dirent.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>


int main(int argc, char **argv) {
    //Timing Part 1
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

    //Get Input Data
    pid_t cpid;
    int buffer_size = atoi(argv[1]);
    int num_prod = atoi(argv[2]);
    int num_cons = atoi(argv[3]);
    int delay = 1000*atoi(argv[4]);
    char* pic = argv[5];

    //Semaphores
    int prod_sem_id = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int cons_sem_id = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    sem_t *prod_sem = shmat(prod_sem_id, NULL, 0);
    sem_t *cons_sem = shmat(cons_sem_id, NULL, 0);
    sem_init(prod_sem, 1, buffer_size);
    sem_init(cons_sem, 1, 0);

    //Mutex
    pthread_mutex_t * mutex = NULL;
    pthread_mutexattr_t attrmutex;
    pthread_mutexattr_init(&attrmutex);
    pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);
    int mutex_id = shmget(IPC_PRIVATE, sizeof(pthread_mutex_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    mutex = shmat(mutex_id, NULL, 0);
    pthread_mutex_init(mutex, &attrmutex);

    //Producers
    CURL *curl_handle;
    CURLcode res;
    char url[256] = "http://ece252-1.uwaterloo.ca:2530/image?img=n&part=";
    url[44] = pic[0];
    RECV_BUF **p_shm_recv_buf = malloc(sizeof(RECV_BUF*) * buffer_size);
    int *shmid = malloc(buffer_size*sizeof(int));
    int shm_size = sizeof_shm_recv_buf(BUF_SIZE);
    //char fname[256];
    pid_t pid = getpid();

    printf("shm_size = %d.\n", shm_size);
    for(int t = 0; t < buffer_size; t++) {
        printf("getting %dth shm\n",t);
        shmid[t] = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
        if ( shmid[t] == -1 ) {
            perror("shmget");
            abort();
        }
        printf("buffer_size: %d, shmid: %d\n", buffer_size, shmid[t]);

        //void * shmat_return = shmat(shmid[t], NULL, 0);
        p_shm_recv_buf[t] = shmat(shmid[t], NULL, 0);
        printf("got %dth shm\n", t);
        shm_recv_buf_init(p_shm_recv_buf[t], BUF_SIZE);
    }

    printf("get seq_shmid\n");
    int seq_shmid = shmget(IPC_PRIVATE, STRIP_NUM*sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int *p_seq_shm = shmat(seq_shmid, NULL, 0);
    memset(p_seq_shm, 0, STRIP_NUM*sizeof(int));
    
    printf("%s: URL is %s\n", argv[0], url);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    for(int f = 0; f < num_prod; f++) {
        cpid = fork();
        if(cpid == 0) {
            pid == getpid();
            /* init a curl session */
            curl_handle = curl_easy_init();
            if (curl_handle == NULL) {
                printf(stderr, "curl_easy_init: returned NULL\n");
                return 1;
            }
            break;
        }
    }

    int p = 0;
    while(p < STRIP_NUM && cpid == 0) {
        pthread_mutex_lock(mutex);
        if(p_seq_shm[p] != 0) {
            printf("%d has already been produced\n", p);
            pthread_mutex_unlock(mutex);
            p++;
        } else {
            p_seq_shm[p] = 1;
            pthread_mutex_unlock(mutex);

            sem_wait(prod_sem);

            int q = 0;
            while(q < buffer_size) {
                pthread_mutex_lock(mutex);
                if(p_shm_recv_buf[q]->seq == -1) {
                    p_shm_recv_buf[q]->seq = -2;
                    pthread_mutex_unlock(mutex);
                    break;
                }
                pthread_mutex_unlock(mutex);
                q++;
            }

            /*int cont = 1;
            int q = 0;
            while(cont) {
                q = 0;
                while(q < buffer_size) {
                    if(p_shm_recv_buf[q]->seq == -1) {
                        break;
                    }
                    q++;
                }
                if(q != buffer_size) {
                    cont = 0;
                }
            }*/

            char temp_url[256];
            sprintf(temp_url, "%s%d", url, p);
            //url[51] = (char)(p+48);
            printf("%s: new URL is %s\n", argv[0], temp_url);

            /* specify URL to get */
            curl_easy_setopt(curl_handle, CURLOPT_URL, temp_url);

            /* register write call back function to process received data */
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl); 
            /* user defined data structure passed to the call back function */
            pthread_mutex_lock(mutex);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)p_shm_recv_buf[q]);
            pthread_mutex_unlock(mutex);

            /* register header call back function to process received header data */
            curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
            /* user defined data structure passed to the call back function */
            pthread_mutex_lock(mutex);
            curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)p_shm_recv_buf[q]);
            pthread_mutex_unlock(mutex);

            /* some servers requires a user-agent field */
            curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

            
            /* get it! */
            pthread_mutex_lock(mutex);
            res = curl_easy_perform(curl_handle);
            pthread_mutex_unlock(mutex);

            if( res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                printf("%lu bytes received in memory %p, seq=%d.\n",  \
                    p_shm_recv_buf[q]->size, p_shm_recv_buf[q]->buf, p_shm_recv_buf[q]->seq);
                
            }

            sem_post(cons_sem);
        }
    }

    //sprintf(fname, "./output_%d_%d.png", p_shm_recv_buf->seq, pid);
    //write_file(fname, p_shm_recv_buf->buf, p_shm_recv_buf->size);

    // Producer Cleanup
    if(cpid == 0) {
        curl_easy_cleanup(curl_handle);
        return 0;
    }

    // Consumers
    U32 width_val = 400;

    int all_shmid = shmget(IPC_PRIVATE, 3*STRIP_NUM*BUF_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    char *p_all_shm = shmat(all_shmid, NULL, 0);
    int sizes_shmid = shmget(IPC_PRIVATE, (STRIP_NUM+1)*sizeof(U64), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    U64 *p_sizes_shm = shmat(sizes_shmid, NULL, 0);
    int sample_shmid = shmget(IPC_PRIVATE, BUF_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    char *p_sample_shm = shmat(sample_shmid, NULL, 0);
    memset(p_sample_shm, 0, BUF_SIZE);

    for(int f = 0; f < num_cons; f++) {
        cpid = fork();
        if(cpid == 0) {
            pid == getpid();
            break;
        }
    }

    while(cpid == 0) {
        /*for(int h = 0; h < STRIP_NUM; h++) {
            pthread_mutex_lock(mutex);
            if(p_sizes_shm[h] == 0) {
                pthread_mutex_lock(mutex);
                break;
            } else if(h == 49) {
                return 0;
            }
        }*/
        pthread_mutex_lock(mutex);
        //printf("num processed: %d\n", p_sizes_shm[STRIP_NUM]);
        if(p_sizes_shm[STRIP_NUM] != STRIP_NUM) {
            p_sizes_shm[STRIP_NUM]++;
            pthread_mutex_unlock(mutex);
        } else {
            pthread_mutex_unlock(mutex);
            return 0;
        }

        sem_wait(cons_sem);
        usleep(delay);
        int g = 0;
        while(g < buffer_size) {
            pthread_mutex_lock(mutex);
            //printf("second slot has: %d with size: %d\n", p_shm_recv_buf[1]->seq, p_sizes_shm[p_shm_recv_buf[1]->seq]);
            if(p_shm_recv_buf[g]->seq != -1 && p_sizes_shm[p_shm_recv_buf[g]->seq] == 0) {
                p_sizes_shm[p_shm_recv_buf[g]->seq] = -1;
                pthread_mutex_unlock(mutex);
                break;
            }
            pthread_mutex_unlock(mutex);
            g++;
        }

        if(g == buffer_size) {
            printf("wtf\n");
        }

        // Sample
        pthread_mutex_lock(mutex);
        if(p_sample_shm[0] == 0) {
            memcpy(p_sample_shm, p_shm_recv_buf[g]->buf, p_shm_recv_buf[g]->size);
        }
        pthread_mutex_unlock(mutex);

        U8 *p_height = malloc(sizeof(U8)*4);
        U64 part_height = 0;
        U8 *length = malloc(sizeof(U8)*4);
        U64 part_length = 0;
        U8 *data = NULL;
        U8 *part_u_data = NULL;
        U64 part_u_data_length = 0;

        pthread_mutex_lock(mutex);
        memcpy(p_height, p_shm_recv_buf[g]->buf + 20, sizeof(U8)*4);
        pthread_mutex_unlock(mutex);
        memcpy(&part_height, p_height, sizeof(part_height));
        part_height = (U64)ntohl(part_height);
        pthread_mutex_lock(mutex);
        memcpy(length, p_shm_recv_buf[g]->buf + 20 + 4 + 9, sizeof(U8)*4);
        pthread_mutex_unlock(mutex);
        memcpy(&part_length, length, sizeof(part_length));
        part_length = (U64)ntohl(part_length);

        data = malloc(sizeof(U8)*part_length);
        pthread_mutex_lock(mutex);
        memcpy(data, p_shm_recv_buf[g]->buf + 20 + 4 + 9 + 4 + 4, part_length);
        pthread_mutex_unlock(mutex);

        part_u_data = malloc(sizeof(U8)*part_height*(width_val*4 + 1));
        memset(part_u_data, 0, sizeof(U8)*part_height*(width_val*4 + 1));
        mem_inf(part_u_data, &part_u_data_length, data, part_length);

        pthread_mutex_lock(mutex);
        p_sizes_shm[p_shm_recv_buf[g]->seq] = part_u_data_length;
        pthread_mutex_unlock(mutex);

        U64 before_cur = 0;
        U64 after_cur = 0;
        for(int a = 0; a < STRIP_NUM; a++) {
            pthread_mutex_lock(mutex);
            if(a < p_shm_recv_buf[g]->seq && p_sizes_shm[a] > 0) {
                before_cur += p_sizes_shm[a];
            } else if (a > p_shm_recv_buf[g]->seq && p_sizes_shm[a] > 0) {
                after_cur += p_sizes_shm[a];
            }
            pthread_mutex_unlock(mutex);
        }
        char *p_temp = malloc(3*STRIP_NUM*BUF_SIZE);
        pthread_mutex_lock(mutex);
        memcpy(p_temp, p_all_shm, 3*STRIP_NUM*BUF_SIZE);
        pthread_mutex_unlock(mutex);
        for(int b = 0; b < before_cur + after_cur + part_u_data_length; b++) {
            pthread_mutex_lock(mutex);
            if(b < before_cur) {
                p_all_shm[b] = p_temp[b];
            } else if(b < before_cur + part_u_data_length) {
                p_all_shm[b] = part_u_data[b - before_cur];
            } else {
                p_all_shm[b] = p_temp[b - part_u_data_length];
            }
            pthread_mutex_unlock(mutex);
        }
        
        free(p_height);
        free(length);
        free(data);
        free(part_u_data);
        free(p_temp);
        pthread_mutex_lock(mutex);
        printf("%dth seq processed\n", p_shm_recv_buf[g]->seq);
        shm_recv_buf_init(p_shm_recv_buf[g], BUF_SIZE);
        pthread_mutex_unlock(mutex);

        sem_post(prod_sem);
    }

    //Parent
    // Stuff for Parent
    U8 *headerlength = malloc(sizeof(U8)*12);
    U8 *IHDRtype = malloc(sizeof(U8)*4);
    U8 *width = malloc(sizeof(U8)*4);
    //U32 width_val = 0;
    U8 *height = malloc(sizeof(U8)*4);
    U32 height_val = htonl(300);
    memcpy(height, &height_val, sizeof(U8)*4);
    height_val = ntohl(height_val);
    U8 *after_height = malloc(sizeof(U8)*5);
    U8 *IHDRcrc = malloc(sizeof(U8)*4);
    //memset(IHDRcrc, 0, sizeof(U8)*4);
    //printf("bytes copied: %d\n", sizeof(IHDRcrc));
    U8 *IDATlength = malloc(sizeof(U8)*4);
    U32 total_length = 0;
    U8 *IDATtype = malloc(sizeof(U8)*4);
    U32 u_data_len = 0;
    U8 *IDATdata = NULL;
    U8 *IDATcrc = malloc(sizeof(U8)*4);
    U8 *IEND = malloc(sizeof(U8)*12);

    for(int w = 0; w < num_prod + num_cons; w++) {
        wait(NULL);
        printf("%dth child done\n", w);
    }
    for(int s = 0; s < STRIP_NUM; s++) {
        u_data_len += p_sizes_shm[s];
    }

    memcpy(headerlength, p_sample_shm, sizeof(U8)*12);
    memcpy(IHDRtype, p_sample_shm + 12, sizeof(U8)*4);
    memcpy(width, p_sample_shm + 12 + 4, sizeof(U8)*4);
    memcpy(after_height, p_sample_shm + 12 + 4 + 4 + 4, sizeof(U8)*5);
    U8 *f_skip_len = malloc(sizeof(U8)*4);
    U32 skip_len = 0;
    memcpy(f_skip_len, p_sample_shm + 12 + 4 + 4 + 4 + 5 + 4, sizeof(U8)*4);
    memcpy(&skip_len, f_skip_len, sizeof(U8)*4);
    skip_len = (U32)ntohl(skip_len);
    //printf("skipped: %d\n", skip_len);
    memcpy(IDATtype, p_sample_shm + 12 + 4 + 4 + 4 + 5 + 4 + 4, sizeof(U8)*4);
    memcpy(IEND, p_sample_shm + 12 + 4 + 4 + 4 + 5 + 4 + 4 + 4 + skip_len + 4, sizeof(U8)*12);

    free(f_skip_len);
    //memcpy(&width_val, width, sizeof(width_val));
    //width_val = (U32)ntohl(width_val);

    //printf("5\n");
    IDATdata = malloc(sizeof(U8) * u_data_len);
    U64 *temp_size = malloc(sizeof(U64));
    mem_def(IDATdata, temp_size, p_all_shm, u_data_len, -1);
    //printf("size of data: %d\n", *temp_size);
    memcpy(&total_length, temp_size, sizeof(total_length));

    total_length = htonl(total_length);
    memcpy(IDATlength, &total_length, sizeof(total_length));
    total_length = (U32)ntohl(total_length);
    //printf("new height: %d\n", height_val);
    //printf("new length: %d\n", *temp_size);
    //printf("u data len: %d\n", u_data_len);
    //total_length = (U32)htonl(total_length);
    height_val = (U32)htonl(height_val);
    memcpy(height, &height_val, sizeof(height_val));

    free(temp_size);

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
    memcpy(IHDRcrc, &temp_crc, sizeof(U8)*4);
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
    memcpy(IDATcrc, &temp_crc, sizeof(U8)*4);

    //File Writing
    char outname[10] = "./all.png";
    FILE *outfile = fopen(outname, "wb+");
    if(outfile == NULL) {
        printf("null\n");
    }
    fwrite(headerlength, 1, 12, outfile);
    fwrite(IHDRtypedata, 1, 17, outfile);
    fwrite(IHDRcrc, 1, 4, outfile);
    fwrite(IDATlength, 1, 4, outfile);
    fwrite(IDATtypedata, 1, 4+total_length, outfile);
    fwrite(IDATcrc, 1, 4, outfile);
    fwrite(IEND, 1, 12, outfile);

    fclose(outfile);

    // Parent Cleanup
    pthread_mutex_destroy(mutex);
    pthread_mutexattr_destroy(&attrmutex); 
    curl_global_cleanup();
    for(int t = 0; t < buffer_size; t++) {
        shmdt(p_shm_recv_buf[t]);
        shmctl(shmid[t], IPC_RMID, NULL);
    }
    shmdt(p_seq_shm);
    shmdt(p_all_shm);
    shmdt(p_sizes_shm);
    shmdt(p_sample_shm);
    shmdt(prod_sem);
    shmdt(cons_sem);
    shmdt(mutex);
    shmctl(seq_shmid, IPC_RMID, NULL);
    shmctl(all_shmid, IPC_RMID, NULL);
    shmctl(sizes_shmid, IPC_RMID, NULL);
    shmctl(sample_shmid, IPC_RMID, NULL);
    shmctl(prod_sem_id, IPC_RMID, NULL);
    shmctl(cons_sem_id, IPC_RMID, NULL);
    shmctl(mutex_id, IPC_RMID, NULL);

    free(width);
    free(height);
    free(after_height);
    free(IHDRcrc);
    free(IDATlength);
    free(IDATtype);
    free(IDATdata);
    free(IDATcrc);
    free(IEND);
    free(IHDRtypedata);
    free(IDATtypedata);
    free(headerlength);
    free(IHDRtype);

    //Timing Part 2
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("%s execution time: %.6lf seconds\n", argv[0],  times[1] - times[0]);

    return 0;
}