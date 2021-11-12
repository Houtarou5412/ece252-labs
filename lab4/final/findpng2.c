#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <search.h>

#define BUF_SIZE 100000
typedef struct list{
    char *url;
    struct list *p_next;
}list;

//GLOBAL
list *png_head;
list *urls_to_check_head;
list *visited_urls_head;
int log = 0;
int pngs_found = 0;
int max_pngs = 50;
int waiting = 0;
pthread_mutex_t mutex;
sem_t url_avail;

void pop_head(list *head) {
    free(head.url);
    list *temp = head->p_next;
    free(head);
    head = temp;
    return;
}

void push_head(list *head) {
    list *temp = malloc(sizeof(list));
    temp->p_next = head;
    head = temp;
    return;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url) // gets the URL
{

    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;
		
    if (buf == NULL) {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
                ENTRY e;
                e.key = href;
                if(hsearch(e, FIND) == NULL) {
                    hsearch(e, ENTER);

                    pthread_mutex_lock(&mutex);
                    push_head(urls_to_check_head);
                    urls_to_check_head->url = malloc(sizeof(e.key));
                    memcpy(urls_to_check_head->url, e.key, sizeof(e.key));
                    pthread_mutex_unlock(&mutex);
                }
            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    int follow_relative_link = 1;
    char *url = NULL; 
    pid_t pid =getpid();

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url);
    return;
}

void process_png(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    pid_t pid =getpid();
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL) {
        //printf("The PNG url is: %s\n", eurl);
        pthread_mutex_lock(&mutex);
        push_head(png_head);
        png_head->url = malloc(sizeof(eurl));
        memcpy(png_head->url, eurl, sizeof(eurl));
        pngs_found++;
        pthread_mutex_unlock(&mutex);
    }

    return;
}
/**
 * @brief process teh download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data. 
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    pid_t pid =getpid();
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
	    printf("Response code: %ld\n", response_code);
    }

    if ( response_code >= 400 ) { 
    	fprintf(stderr, "Error.\n");
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    	printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
        fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        process_html(curl_handle, p_recv_buf);
    } else if ( strstr(ct, CT_PNG) ) {
        process_png(curl_handle, p_recv_buf);
    }

    return 0;
}

void *check_urls(void *ignore) {
    RECV_BUF recv;
    CURL *curl_handle = easy_handle_init(&recv, NULL);
    while(png_found < max_pngs) {

        ENTRY e;
        CURLcode res;
        char *content_type;

        pthread_mutex_lock(&mutex);
        waiting++;
        pthread_mutex_unlock(&mutex);

        sem_wait(&url_avail);

        pthread_mutex_lock(&mutex);
        waiting--;
        e.key = urls_to_check_head->url;
        /*if(hsearch(e, FIND) == NULL) {
            hsearch(e, ENTER);
        } else {
            pthread_mutex_unlock(&mutex);
            continue;
        }*/
        pop_head(urls_to_check_head);
        curl_easy_setopt(curl_handle, CURLOPT_URL, e.key);

        if(visited_urls_head == NULL && log) {
            visited_urls_head = malloc(sizeof(list));
        } else {
            push_head(visited_urls_head);
        }

        if(visited_urls_head == NULL) {
            visited_urls_head->url = malloc(sizeof(e.key));
            memcpy(visited_urls_head->url, e.key, sizeof(e.key));
        }

        pthread_mutex_unlock(&mutex);

        res = curl_easy_perform(curl_handle);

        curl_easy_get_info(curl_handle, CURLINFO_CONTENT_TYPE, &content_type);

        process_data(curl_handle, &recv);

        /*if(strcmp(content_type, "image/png") == 0) {
            char *temp = malloc(8);
            memcpy(temp, recv.buf, recv.size);
            if(strcmp(temp, 0x89504E470D0A1A1)) {
                pthread_mutex_lock(&mutex);
                push_head(png_head);
                png_head->url = malloc(sizeof(e.key));
                memcpy(png_head->url, e.key, sizeof(e.key));
                pthread_mutex_unlock(&mutex);
            }
        } else if(strcmp(content_type, "text/html") == 0) {

        }*/
    }
    cleanup(curl_handle, recv);
    return;
}

int main(int argc, char **argv) {
    pthread_mutex_init(&mutex);
    sem_init(&url_avail, 0, 0);
    int threads = 1;
    char *logfile = NULL;
    hcreate(200000);
    png_head = malloc(sizeof(list));
    urls_to_check_head = malloc(sizeof(list));
    for(int t = 0; t < argc; t++) {
        if(strcmp(argv[t],"-t")) {
            threads = atoi(argv[t+1]);
            t++;
        }
        else if(strcmp(argv[t], "-m")){
            max_pngs = atoi(argv[t+1]);
            t++;
        }
        else if(strcmp(argv[t], "-v")){
            logfile = argv[t+1];
            log = 1;
            t++;
        } else {
            ENTRY e;
            e.key = argv[t];
            hsearch(e, ENTER);
            urls_to_check_head->url = malloc(sizeof(argv[t]));
            memcpy(urls_to_check_head->url, argv[t], sizeof(argv[t]));
            sem_post(&url_avail);
        }
    }

    p_thread_t *ptids = malloc(threads*sizeof(p_thread_t));
    for(int u = 0; t < threads; t++) {
        p_thread_create(ptids + u, NULL, check_urls, NULL);
    }

    while(pngs_found < max_pngs) {
        if(waiting == 10) {
            for(int g = 0; g < threads; g++) {
                pthread_cancel(ptids[g]);
            }
            break;
        }
    }

    for(int v = 0; v < threads; v++) {
        p_thread_join(ptids[v], NULL);
    }
    char *fname = "./png_urls.txt";
    FILE f = fopen(fname);
    for(int w = 0; w < max_pngs && png_head != NULL; w++) {
        fprintf(&f, "%s\n", png_head->url);
        pop_head(png_head);
    }
    fclose(f);

    while(png_head != NULL) {
        pop_head(png_head);
    }
    
    FILE l;
    if(log) {
        l = fopen(logfile);
    }
    while(visited_urls_head != NULL) {
        if(log) {
            fprintf(&l, "%s\n", visited_urls_head->url);
        }
        pop_head(visited_urls_head);
    }

    while(urls_to_check_head != NULL) {
        pop_head(urls_to_check_head);
    }

    hdestroy();
    sem_destroy(&url_avail);
    pthread_mutex_destroy(&mutex);
    free(ptids);

    return 0;
}
