#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <linux/time.h>
#include <errno.h>

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define PINK "\033[1;35m"
#define ESC "\033[0m"

int r, w, d;
int n, c, T;

typedef struct node {
    int id;
    int file;
    char opr;
    int arrival;
    struct node* next;
} node;

typedef struct linkedlist {
    node* front;
    node* rear;
} linkedlist;

typedef struct file_count {
    int count;
    int write_lock;
    int is_deleted;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} file_count;

file_count* files;
linkedlist* request_queue;

node* createnode(int id, int file, char opr, int arrival) {
    node* new_node = (node*) malloc(sizeof(node));
    new_node->id = id;
    new_node->file = file;
    new_node->opr = opr;
    new_node->arrival = arrival;
    new_node->next = NULL;
    return new_node;
}

linkedlist* append(linkedlist* l, int id, int file, char opr, int arrival) {
    node* new_node = createnode(id, file, opr, arrival);
    if (l->rear == NULL) {
        l->front = new_node;
        l->rear = new_node;
    } else {
        l->rear->next = new_node;
        l->rear = new_node;
    }
    return l;
}

linkedlist* input(linkedlist* l, int* request_count) {
    char cmd[9];
    scanf("%d %d %d", &r, &w, &d);
    scanf("%d %d %d", &n, &c, &T);

    while (1) {
        scanf("%s", cmd);
        if (strcmp(cmd, "STOP") == 0)
            break;
        int uid = atoi(cmd);
        int f, t;
        scanf("%d %s %d", &f, cmd, &t);
        if(f >= n){
            printf(RED"The requested file doesn't exist!\n"ESC);
            continue;
        }
        else if(strcmp(cmd, "READ") && strcmp(cmd, "WRITE") && strcmp(cmd, "DELETE")){
            printf(RED"The requested command doesn't exist\n"ESC);
            continue;
        }
        l = append(l, uid, f, cmd[0], t);
        (*request_count)++;
    }
    return l;
}

void init_files() {
    for (int i = 0; i < n; i++) {
        files[i].count = 0;
        files[i].write_lock = 0;
        files[i].is_deleted = 0;
        pthread_mutex_init(&files[i].mutex, NULL);
        pthread_cond_init(&files[i].cond, NULL);
    }
}

void* handle_request(void* arg) {
    node* request = (node*) arg;
    file_count* file = &files[request->file - 1];
    int request_taken = request->arrival, elapsed_time = 0;
    sleep(request->arrival);
    char operation[10];
    if(request->opr == 'R') strcpy(operation, "READ");
    else if(request->opr == 'W') strcpy(operation, "WRITE");
    else if(request->opr == 'D') strcpy(operation, "DELETE");
    pthread_mutex_lock(&file->mutex);
    printf(YELLOW"User %d has made request for performing %s on file %d at %d seconds\n"ESC, request->id, operation, request->file, request->arrival);
    if(file->is_deleted || file->write_lock == -1){
        printf("LAZY has declined the request of User %d at %d seconds because an invalid/deleted file was requested.\n", request->id, request->arrival);
        pthread_mutex_unlock(&file->mutex);
        return NULL;
    }
    
    struct timespec signal, end, finish;
    request_taken++;
    elapsed_time = 1;
    sleep(1);

    clock_gettime(CLOCK_REALTIME, &end);
    end.tv_sec += T-1;
    if (request->opr == 'R') {
        while(file->count >= c){
            clock_gettime(CLOCK_REALTIME, &signal);
            int wait_result = pthread_cond_timedwait(&file->cond, &file->mutex, &end);
            clock_gettime(CLOCK_REALTIME, &finish);
            elapsed_time += (int)(finish.tv_sec - signal.tv_sec);
            request_taken += (int)(finish.tv_sec - signal.tv_sec);
            if (elapsed_time >= T) {
                printf(RED"User %d canceled the request due to no response at %d seconds\n"ESC, request->id, request->arrival + elapsed_time);
                pthread_mutex_unlock(&file->mutex);
                return NULL;
            }
            else if(file->count < c) break;
            else if(file->is_deleted == 1){
                printf("LAZY has declined the request of User %d at %d seconds because an invalid/deleted file was requested.\n", request->id, request->arrival + elapsed_time);
                pthread_mutex_unlock(&file->mutex);
                return NULL;
            }
        }
        file->count++;
        printf(PINK"LAZY has taken up the request of User %d at %d seconds\n"ESC, request->id, request_taken);
        pthread_mutex_unlock(&file->mutex);
        sleep(r);
        pthread_mutex_lock(&file->mutex);
        printf(GREEN"The request for User %d was completed at %d seconds\n"ESC, request->id, request_taken + r);
        file->count--;
        pthread_cond_signal(&file->cond);
        pthread_mutex_unlock(&file->mutex);
        return NULL;
    }
    else if (request->opr == 'W') {
        while (file->count != 0 || file->write_lock == 1) {
            clock_gettime(CLOCK_REALTIME, &signal);
            int wait_result = pthread_cond_timedwait(&file->cond, &file->mutex, &end);
            clock_gettime(CLOCK_REALTIME, &finish);
            elapsed_time += (int)(finish.tv_sec - signal.tv_sec);
            request_taken += (int)(finish.tv_sec - signal.tv_sec);
            if (elapsed_time >= T) {
                printf(RED"User %d canceled the request due to no response at %d seconds\n"ESC, request->id, request->arrival + elapsed_time);
                pthread_mutex_unlock(&file->mutex);
                return NULL;
            }
            else if(file->count == 0 && file->write_lock == 0) break;
            else if(file->is_deleted == 1){
                printf("LAZY has declined the request of User %d at %d seconds because an invalid/deleted file was requested.\n", request->id, request->arrival + elapsed_time);
                pthread_mutex_unlock(&file->mutex);
                return NULL;
            }
        }
        file->count++;
        file->write_lock = 1;
        printf(PINK"LAZY has taken up the request of User %d at %d seconds\n"ESC, request->id, request->arrival + elapsed_time);
        pthread_mutex_unlock(&file->mutex);
        sleep(w);
        pthread_mutex_lock(&file->mutex);
        printf(GREEN"The request for User %d was completed at %d seconds\n"ESC, request->id, request_taken + w);
        file->write_lock = 0;
        file->count--;
        pthread_cond_signal(&file->cond);
        pthread_mutex_unlock(&file->mutex);
        return NULL;
    }
    else if (request->opr == 'D') {
        while(file->count){
            clock_gettime(CLOCK_REALTIME, &signal);
            int wait_result = pthread_cond_timedwait(&file->cond, &file->mutex, &end);
            clock_gettime(CLOCK_REALTIME, &finish);
            elapsed_time += (int)(finish.tv_sec - signal.tv_sec);
            request_taken += (int)(finish.tv_sec - signal.tv_sec);
            if (elapsed_time >= T){
                printf(RED"User %d canceled the request due to no response at %d seconds\n"ESC, request->id, request_taken);
                pthread_mutex_unlock(&file->mutex);
                return NULL;
            }
            else if(file->count == 0) break;
            else if(file->is_deleted == 1){
                printf("LAZY has declined the request of User %d at %d seconds because an invalid/deleted file was requested.\n", request->id, request->arrival + elapsed_time);
                pthread_mutex_unlock(&file->mutex);
                return NULL;
            }
        }
        file->count++;
        file->is_deleted = 1;
        file->write_lock = -1;
        printf(PINK"LAZY has taken up the request of User %d at %d seconds\n"ESC, request->id, request_taken);
        pthread_mutex_unlock(&file->mutex);
        sleep(d);
        pthread_mutex_lock(&file->mutex);
        printf(GREEN"The request for User %d was completed at %d seconds\n"ESC, request->id, request_taken + d);
        file->write_lock = 0;
        pthread_cond_signal(&file->cond);
        pthread_mutex_unlock(&file->mutex);
        return NULL;
    }
}


int main() {
    freopen("input2.txt", "r", stdin);
    printf("LAZY has woken up!\n");
    int request_count = 0;
    request_queue = (linkedlist*) malloc(sizeof(linkedlist));
    request_queue->front = NULL;
    request_queue->rear = NULL;

    request_queue = input(request_queue, &request_count);

    files = (file_count*) malloc(n * sizeof(file_count));
    init_files();
    node* curr = request_queue->front;
    pthread_t* threads = (pthread_t*) malloc(request_count * sizeof(pthread_t));

    int i = 0;
    while (curr != NULL) {
        pthread_create(&threads[i], NULL, handle_request, (void*) curr);
        curr = curr->next;
        i++;
    }

    for (int j = 0; j < request_count; j++) {
        pthread_join(threads[j], NULL);
    }

    printf("LAZY has no more pending requests and is going back to sleep!\n");

    free(threads);
    free(files);
    while(request_queue->front){
        node* temp = request_queue->front;
        request_queue->front = request_queue->front->next;
        free(temp);
    }
    free(request_queue);
    fclose(stdin);
    return 0;
}