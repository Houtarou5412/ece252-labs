#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <search.h>
#include <pthread.h>
#include "curl.h"

typedef struct list{
    char *url;
    struct list *p_next;
}list;

//GLOBAL
list *png_head = NULL;
list *urls_to_check_head = NULL;
list *visited_urls_head = NULL;
list *hash_urls_head = NULL;
int log_check = 0;
int pngs_found = 0;
int max_pngs = 50;
int waiting = 0;
pthread_mutex_t mutex;
sem_t url_avail;

void pop_head(list **head) {
    //printf("head at %p\nurl: %s at %p\n", *head, (*head)->url, &((*head)->url));
    free((*head)->url);
    list *temp = *head;
    *head = (*head)->p_next;
    //printf("temp at %p, head at %p\n", temp, *head);
    free(temp);

    /*printf("pop_head 1\n");
    free(head->url);
    list *temp = head->p_next;
    printf("pop_head 2\n");
    free(head);
    printf("pop_head 3\n");
    head = temp;*/
    return;
}

void push_head(list **head) {
    //printf("push 1\n");
    list *temp = malloc(sizeof(list));
    temp->p_next = *head;
    *head = temp;
    return;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url) {
    //printf("find_http 1\n");
    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;
		
    if (buf == NULL) {
        return 1;
    }

    //printf("find_http 2\n");

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        //printf("find_http 3\n");
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {

            //printf("find_http 4\n");

            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }

            //printf("find_http 5\n");

            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
                //printf("find_http 5.1\n");
                ENTRY e;
                e.key = malloc(strlen((char *)href) + 1);
                memcpy(e.key, (char *)href, strlen((char *)href) + 1);
                if(hsearch(e, FIND) == NULL) {
                    //printf("new key: %s\n",e.key);
                    hsearch(e, ENTER);

                    pthread_mutex_lock(&mutex);
                    push_head(&urls_to_check_head);
                    urls_to_check_head->url = malloc(strlen(e.key)+1);
                    memcpy(urls_to_check_head->url, e.key, strlen(e.key)+1);

                    push_head(&hash_urls_head);
                    hash_urls_head->url = malloc(strlen(e.key)+1);
                    memcpy(hash_urls_head->url, e.key, strlen(e.key)+1);

                    //printf("new first url: %s\n", urls_to_check_head->url);
                    sem_post(&url_avail);
                    pthread_mutex_unlock(&mutex);
                }
            }

            //printf("find_http 6\n");

            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }

    //printf("find_http 7\n");

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}

void process_html(CURL *curl_handle, RECV_BUF *p_recv_buf) {
    //printf("process_html 1\n");
    int follow_relative_link = 1;
    char *url = NULL; 
    //pid_t pid =getpid();

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);

    //printf("process_html 2\n");

    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url);

    //printf("process_html 3\n");
    return;
}

void process_png(CURL *curl_handle, RECV_BUF *p_recv_buf) {
    //printf("process_png 1\n");
    //pid_t pid =getpid();
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL) {
        //printf("process_png 2\n");
        //printf("The PNG url is: %s\n", eurl);
        pthread_mutex_lock(&mutex);
        push_head(&png_head);
        png_head->url = malloc(strlen(eurl)+1);
        memcpy(png_head->url, eurl, strlen(eurl)+1);
        pngs_found++;
        pthread_mutex_unlock(&mutex);
    }
    //printf("process_png 3\n");

    return;
}
/**
 * @brief process teh download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data. 
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf) {
    //printf("process_data 3\n");
    CURLcode res;
    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    	//printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
        printf("Failed obtain Content-Type\n");
        return 2;
    }

    //printf("process_data 4\n");

    if ( strstr(ct, CT_HTML) ) {
        printf("html\n");
        process_html(curl_handle, p_recv_buf);
    } else if ( strstr(ct, CT_PNG) ) {
        printf("png\n");
        process_png(curl_handle, p_recv_buf);
    }

    //printf("process_data 5\n");

    return 0;
}

void *check_urls(void *ignore) {
    //printf("check_urls 1\n");
    RECV_BUF recv;
    CURL *curl_handle = easy_handle_init(&recv, NULL);
    while(pngs_found < max_pngs) {

        //printf("check_urls 1.1\n");
        ENTRY e;
        CURLcode res;
        //char *content_type;

        pthread_mutex_lock(&mutex);
        printf("pngs_found: %d\n", pngs_found);
        waiting++;
        pthread_mutex_unlock(&mutex);

        sem_wait(&url_avail);

        pthread_mutex_lock(&mutex);
        waiting--;

        //printf("check_urls 1.2\n");

        e.key = malloc(strlen(urls_to_check_head->url)+1);
        //printf("e.key: %p\n", e.key);
        memcpy(e.key, urls_to_check_head->url, strlen(urls_to_check_head->url)+1);
        /*if(hsearch(e, FIND) == NULL) {
            hsearch(e, ENTER);
        } else {
            pthread_mutex_unlock(&mutex);
            continue;
        }*/

        //printf("urls_to_check_head %p, e.key %s at %p\n", urls_to_check_head, e.key, &(e.key));

        pop_head(&urls_to_check_head);
        //printf("1.3\n");
        //printf("%s\n", e.key);
        curl_easy_setopt(curl_handle, CURLOPT_URL, e.key);
        //printf("%s\n", e.key);

        
        push_head(&visited_urls_head);
        visited_urls_head->url = malloc(strlen(e.key)+1);
        memcpy(visited_urls_head->url, e.key, strlen(e.key)+1);
        

        //printf("check_urls 2\n");

        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK ) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            pthread_mutex_unlock(&mutex);
            continue;
        } else {
            //printf("%lu bytes received in memory %p, seq=%d.\n", recv_buf.size, recv_buf.buf, recv_buf.seq);
        }

        //printf("process_data 1\n");
        long response_code = 300;
        int ignore = 0;

        while(response_code >= 300 && !ignore) {
            //printf("redirecting\n");
            res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
            if ( res == CURLE_OK ) {
                //printf("Response code: %ld\n", response_code);
            }

            //printf("process_data 2\n");

            if ( response_code >= 400 ) { 
                printf("Error in response code.\n");
                ignore = 1;
            } else if( response_code >= 300 ) {
                //printf("rcode 3xx, e.key %p\n", e.key);
                //free(e.key);
                //printf("get redirect url\n");
                char * temp_key;
                curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_URL, &temp_key);
                e.key = malloc(strlen(temp_key) + 1);
                memcpy(e.key, temp_key, strlen(temp_key) + 1);
                if(hsearch(e, FIND) == NULL) {
                    hsearch(e, ENTER);
                    
                    push_head(&visited_urls_head);
                    visited_urls_head->url = malloc(strlen(e.key)+1);
                    memcpy(visited_urls_head->url, e.key, strlen(e.key)+1);
                    
                    push_head(&hash_urls_head);
                    hash_urls_head->url = malloc(strlen(e.key)+1);
                    memcpy(hash_urls_head->url, e.key, strlen(e.key)+1);

                    curl_easy_setopt(curl_handle, CURLOPT_URL, e.key);
                    res = curl_easy_perform(curl_handle);

                    if( res != CURLE_OK ) {
                        printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                        ignore = 1;
                    } else {
                        //printf("%lu bytes received in memory %p, seq=%d.\n", recv_buf.size, recv_buf.buf, recv_buf.seq);
                    }

                } else {
                    //printf("found e.key %s\n", e.key);
                    ignore = 1;
                }
            } else {
                //printf("rcode 2xx\n");
            }
        }
        

        pthread_mutex_unlock(&mutex);

        if(recv.size == 0) {
            ignore = 1;
        }
        //printf("check_urls 3\n");

        //curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &content_type);

        if(!ignore) {
            //printf("start processing\n");
            process_data(curl_handle, &recv);
        }

        //printf("check_urls 4\n");

        /*if(strcmp(content_type, "image/png") == 0) {
            char *temp = malloc(8);
            memcpy(temp, recv.buf, recv.size);
            if(strcmp(temp, 0x89504E470D0A1A1) == 0) {
                pthread_mutex_lock(&mutex);
                push_head(&png_head);
                png_head->url = malloc(sizeof(e.key));
                memcpy(png_head->url, e.key, sizeof(e.key));
                pthread_mutex_unlock(&mutex);
            }
        } else if(strcmp(content_type, "text/html") == 0) {

        }*/

        //free(e.key);
    }
    cleanup(curl_handle, &recv);
    return NULL;
}

int main(int argc, char **argv) {
    pthread_mutex_init(&mutex, NULL);
    sem_init(&url_avail, 0, 0);
    int threads = 1;
    char *logfile = NULL;
    hcreate(200000);
    push_head(&png_head);

    printf("main 1\n");
    for(int t = 1; t < argc; t++) {
        printf("main 1.1\n");
        if(strcmp(argv[t],"-t") == 0) {
            threads = atoi(argv[t+1]);
            t++;
        }
        else if(strcmp(argv[t], "-m") == 0){
            max_pngs = atoi(argv[t+1]);
            t++;
        }
        else if(strcmp(argv[t], "-v") == 0){
            logfile = argv[t+1];
            log_check = 1;
            t++;
        } else {
            printf("ok\n");
            //printf("%p -> %p\n", urls_to_check_head, urls_to_check_head->p_next);
            ENTRY e;
            e.key = argv[t];
            hsearch(e, ENTER);

            push_head(&urls_to_check_head);
            urls_to_check_head->url = malloc(strlen(argv[t])+1);
            memcpy(urls_to_check_head->url, argv[t], strlen(argv[t])+1);

            push_head(&hash_urls_head);
            hash_urls_head->url = malloc(strlen(argv[t])+1);
            memcpy(hash_urls_head->url, argv[t], strlen(argv[t])+1);

            sem_post(&url_avail);
            printf("%s %s %s\n",argv[t], e.key, urls_to_check_head->url);
        }
    }

    printf("main 2\n");

    pthread_t *ptids = malloc(threads*sizeof(pthread_t));
    for(int u = 0; u < threads; u++) {
        pthread_create(ptids + u, NULL, check_urls, NULL);
        //printf("thread %d created\n", u);
    }

    printf("main 3\n");

    int make_sure = 0;
    while(pngs_found < max_pngs) {
        if(waiting == threads && make_sure) {
            printf("cancelling\n");
            for(int g = 0; g < threads; g++) {
                pthread_cancel(ptids[g]);
                //pthread_mutex_trylock(&mutex);
                //pthread_mutex_unlock(&mutex);
            }
            break;
        } else if(waiting == threads) {
            sleep(5);
            make_sure = 1;
        } else {
            make_sure = 0;
        }
    }

    printf("main 4\n");

    for(int v = 0; v < threads; v++) {
        pthread_join(ptids[v], NULL);
    }

    printf("main 5\n");

    char *fname = "./png_urls.txt";
    FILE *f = fopen(fname, "w+");
    for(int w = 0; w < max_pngs && png_head != NULL; w++) {
        fprintf(f, "%s\n", png_head->url);
        pop_head(&png_head);
    }
    fclose(f);

    printf("main 6\n");

    while(png_head != NULL) {
        pop_head(&png_head);
    }

    printf("main 7\n");
    
    FILE *l;
    if(log_check) {
        l = fopen(logfile, "w+");
    }
    while(visited_urls_head != NULL) {
        if(log_check) {
            fprintf(l, "%s\n", visited_urls_head->url);
        }
        pop_head(&visited_urls_head);
    }
    if(log_check) {
        fclose(l);
    }

    printf("main 8\n");

    while(urls_to_check_head != NULL) {
        pop_head(&urls_to_check_head);
    }

    printf("main 9\n");

    while(hash_urls_head != NULL) {
        ENTRY s;
        s.key = hash_urls_head->url;
        ENTRY *entry = hsearch(s, FIND);
        if(entry != NULL) {
            free(entry->key);
        } else {
            printf("wtf\n");
        }
        
        pop_head(&hash_urls_head);
    }

    printf("main 10\n");

    hdestroy();
    sem_destroy(&url_avail);
    pthread_mutex_destroy(&mutex);
    free(ptids);

    return 0;
}

