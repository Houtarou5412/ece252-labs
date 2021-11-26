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
#include "multi_curl.h"

#define MAX_WAIT_MSECS 30*1000 /* Wait max. 30 seconds */

typedef struct list{
    char *url;
    struct list *p_next;
}list;

//GLOBAL
list *png_head = NULL;
list *urls_to_check_head = NULL;
list *visited_urls_head = NULL;
list *hash_urls_head = NULL;
int num_urls_to_check = 0;
int threads = 1;
int log_check = 0;
int pngs_found = 0;
int max_pngs = 50;
int waiting = 0;
int maybe_png = 0;
int early_cancel = 0;

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
    temp->url = NULL;
    *head = temp;
    return;
}

int is_png(char *png) {
    return (unsigned char)png[0] == 137 && (unsigned char)png[1] == 80 && (unsigned char)png[2] == 78 && (unsigned char)png[3] == 71 && (unsigned char)png[4] == 13 && (unsigned char)png[5] == 10 && (unsigned char)png[6] == 26 && (unsigned char)png[7] == 10 ? 1 : 0;
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

                    num_urls_to_check++;
                    push_head(&urls_to_check_head);
                    urls_to_check_head->url = malloc(strlen(e.key)+1);
                    memcpy(urls_to_check_head->url, e.key, strlen(e.key)+1);

                    push_head(&hash_urls_head);
                    hash_urls_head->url = malloc(strlen(e.key)+1);
                    memcpy(hash_urls_head->url, e.key, strlen(e.key)+1);

                } else {
                    free(e.key);
                    //printf("existing key: %s\n",e.key);
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

    if ( eurl != NULL && is_png(p_recv_buf->buf)) {
        //printf("process_png 2\n");
        //printf("The PNG url is: %s\n", eurl);
        push_head(&png_head);
        png_head->url = malloc(strlen(eurl)+1);
        memcpy(png_head->url, eurl, strlen(eurl)+1);
        pngs_found++;
    }

    return;
}
/**
 * @brief 
 * 
 *
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
        //printf("Failed obtain Content-Type\n");
        return 2;
    }

    //printf("process_data 4\n");

    if ( strstr(ct, CT_HTML) ) {
        //printf("html\n");
        process_html(curl_handle, p_recv_buf);
    } else if ( strstr(ct, CT_PNG) ) {
        //printf("png\n");
        process_png(curl_handle, p_recv_buf);
    }

    //printf("process_data 5\n");

    return 0;
}

void init(CURLM *cm, int i) {   
    RECV_BUF *recv = malloc(sizeof(RECV_BUF));
    CURL *eh = easy_handle_init(recv);
    
    push_head(&visited_urls_head);
    visited_urls_head->url = malloc(strlen(urls_to_check_head->url)+1);
    memcpy(visited_urls_head->url, urls_to_check_head->url, strlen(urls_to_check_head->url)+1);
    pop_head(&urls_to_check_head);

    curl_easy_setopt(eh, CURLOPT_HEADER, 0L);
    curl_easy_setopt(eh, CURLOPT_URL, visited_urls_head->url);
    curl_easy_setopt(eh, CURLOPT_PRIVATE, recv);
    curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);
    curl_multi_add_handle(cm, eh);
}

void *check_urls(void *ignore) {
    CURLM *cm=NULL;
    CURLMsg *msg=NULL;
    CURLcode return_code=0;

    curl_global_init(CURL_GLOBAL_ALL);

    cm = curl_multi_init();

    printf("check_urls 1\n");
    
    while(urls_to_check_head != NULL && pngs_found < max_pngs) {

        int still_running=0, i=0, msgs_left=0;
        int http_status_code;
        const char *szUrl;

        int concurrencies = 0;
        for (i = 0; i < threads && urls_to_check_head != NULL; ++i) {
            init(cm, i);
            concurrencies++;
        }

        printf("check_urls 2 finished\n");
        curl_multi_perform(cm, &still_running);

        do {
            int numfds=0;
            int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
            if(res != CURLM_OK) {
                fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
                continue;
            }
            /*
            if(!numfds) {
                fprintf(stderr, "error: curl_multi_wait() numfds=%d\n", numfds);
                return EXIT_FAILURE;
            }
            */
            curl_multi_perform(cm, &still_running);

        } while(still_running);

        printf("check http status\n");
        CURL *eh = NULL;

        while ((msg = curl_multi_info_read(cm, &msgs_left))) {
            printf("%d msgs_left\n", msgs_left);
            if (msg->msg == CURLMSG_DONE) {
                printf("message done\n");
                eh = msg->easy_handle;

                return_code = msg->data.result;
                if(return_code!=CURLE_OK) {
                    fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                    continue;
                }

                // Get HTTP status code
                http_status_code=0;
                szUrl = NULL;
                RECV_BUF *recv;

                printf("get infos\n");
                curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
                curl_easy_getinfo(eh, CURLINFO_EFFECTIVE_URL, &szUrl);
                curl_easy_getinfo(eh, CURLINFO_PRIVATE, recv);

                printf("analyze http status\n");
                if(http_status_code >= 400) {
                    printf("http 400 error\n");
                } else if(http_status_code >= 300) {
                    printf("http 300 redirect\n");
                    ENTRY e;
                    e.key = malloc(strlen(szUrl)+1);
                    memcpy(e.key, szUrl, strlen(szUrl)+1);
                    if(!hsearch(e, FIND)) {
                        hsearch(e, ENTER);

                        pop_head(&urls_to_check_head);
                        urls_to_check_head->url = malloc(strlen(e.key)+1);
                        memcpy(urls_to_check_head->url, e.key, strlen(e.key)+1);

                        push_head(&hash_urls_head);
                        hash_urls_head->url = malloc(strlen(e.key)+1);
                        memcpy(hash_urls_head->url, e.key, strlen(e.key)+1);
                    } else {
                        free(e.key);
                    }
                    
                } else if(recv->size != 0 && pngs_found < max_pngs) {
                    printf("http 200 okay\n");
                    process_data(eh, recv);
                }

                curl_multi_remove_handle(cm, eh);
                curl_easy_cleanup(eh);
                recv_buf_cleanup(recv);
            }
            else {
                printf("error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
            }
        }
    }
    curl_multi_cleanup(cm);

    return NULL;
}

int main(int argc, char **argv) {
    //Timing Part 1
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
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
        } else if(t == argc-1) {
            printf("ok\n");
            //printf("%p -> %p\n", urls_to_check_head, urls_to_check_head->p_next);
            ENTRY e;
            e.key = argv[t];
            hsearch(e, ENTER);

            push_head(&urls_to_check_head);
            num_urls_to_check++;
            urls_to_check_head->url = malloc(strlen(argv[t])+1);
            memcpy(urls_to_check_head->url, argv[t], strlen(argv[t])+1);

            push_head(&hash_urls_head);
            hash_urls_head->url = malloc(strlen(argv[t])+1);
            memcpy(hash_urls_head->url, argv[t], strlen(argv[t])+1);
            //printf("%s %s %s\n",argv[t], e.key, urls_to_check_head->url);
        }
    }

    check_urls(NULL);

    printf("main 5\n");

    char *fname = "./png_urls.txt";
    if( access( fname, F_OK ) == 0 ) {
        remove(fname);
    } 
    FILE *f = fopen(fname, "w+");
    for(int w = 0; w < pngs_found && w < max_pngs; w++) {
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
    if( log_check && access( logfile, F_OK ) == 0 ) {
        remove(logfile);
    } 
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
        //printf("p_next = %p\n", hash_urls_head->p_next);
        ENTRY s;
        s.key = hash_urls_head->url;
        ENTRY *entry = hsearch(s, FIND);
        if(entry != NULL && hash_urls_head->p_next != NULL) {
            free(entry->key);
        }
        //printf("entry passed %p\n", hash_urls_head);
        
        pop_head(&hash_urls_head);
    }

    printf("main 10\n");

    hdestroy();

    //Timing Part 2
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("%s execution time: %.6lf seconds\n", argv[0],  times[1] - times[0]);

    return 0;
}

