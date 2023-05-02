#include "os_threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0
#define WAIT_TIME 500


#define ERROR(message) { \
        fprintf(stderr, "An Error has occured with message: %s\n", message); \
        exit(EXIT_FAILURE); \
}

os_task_t *task_create(void *arg, void (*f)(void *)) {

    os_task_t* new_task = NULL;
    size_t task_size = sizeof(os_task_t);
    new_task = calloc(1, task_size);
    if (new_task == NULL) {
        ERROR("Calloc call failed");
    }
    new_task->argument = arg;
    new_task->task = f;

    return new_task;
}


void add_task_in_queue(os_threadpool_t *tp, os_task_t *t) {

    os_task_queue_t *new_queue_task = NULL;
    size_t queue_task_size = sizeof(os_task_queue_t);

    new_queue_task = calloc(1, queue_task_size);
    
    if (new_queue_task == NULL) {
        ERROR("Calloc call failed");
    }

    
    short status = 0;
    status = pthread_mutex_lock(&tp->taskLock);
    if (status) {
        ERROR("Locking mutex failed");
    }
    os_task_queue_t *last_queue_task = tp->tasks;

    if (last_queue_task == NULL) {
        new_queue_task->task = t;
        new_queue_task->next = NULL;
        tp->tasks = new_queue_task;
        pthread_mutex_unlock(&tp->taskLock);
        return;
    }

    new_queue_task->task = t;
    new_queue_task->next = NULL;

    if (last_queue_task->next != NULL) {
        do {
            last_queue_task = last_queue_task->next;
        } while (last_queue_task->next != NULL);
    }

    last_queue_task->next = new_queue_task;
    pthread_mutex_unlock(&tp->taskLock);
    if (status) {
        ERROR("Unlocking mutex failed");
    }
}


os_task_t *get_task(os_threadpool_t *tp) {

    short status = 0;
    status = pthread_mutex_lock(&tp->taskLock);
    if (status) {
        ERROR("Locking mutex failed");
    }
    os_task_t *first_task = NULL;

    if (tp->tasks != NULL) {
        first_task = tp->tasks->task;
        void *to_delete = tp->tasks;
        tp->tasks = tp->tasks->next;
        free(to_delete);    
    }

    status = pthread_mutex_unlock(&tp->taskLock);
    if (status) {
        ERROR("Unlocking mutex failed");
    }
    return first_task;
}

os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads) {

    os_threadpool_t *all_tasks = NULL;
    size_t threadpool_size = sizeof(os_threadpool_t);
    all_tasks = calloc(1, threadpool_size);
    if (all_tasks == NULL) {
        ERROR("Calloc call failed");
    }
    all_tasks->num_threads = nThreads;
    all_tasks->should_stop = FALSE;
    all_tasks->threads = NULL;
    size_t pthread_size = sizeof(pthread_t);
    all_tasks->threads = calloc(nThreads, pthread_size);

    if (all_tasks->threads == NULL) {
        ERROR("Calloc call failed");
    }

    all_tasks->tasks = NULL;
    size_t queue_task_size = sizeof(os_task_queue_t); 
    all_tasks->tasks = calloc(1, queue_task_size);

    if (all_tasks->threads == NULL) {
        ERROR("Calloc call failed");
    }
    all_tasks->tasks->task = NULL;
    all_tasks->tasks->next = NULL;

    short status = 0;
    status = pthread_mutex_init(&all_tasks->taskLock, NULL);
    if (status) {
        ERROR("Initialize mutex failed");
    }

    pthread_t *current_thread = all_tasks->threads;

    while (nThreads) {
        status = pthread_create(current_thread, NULL, thread_loop_function, (void *) all_tasks);
        if (status) {
            ERROR("Pthread create failed");
        }
        ++current_thread;
        --nThreads;
    }

    return all_tasks;
}

void *thread_loop_function(void *args) {
    
    os_threadpool_t *all_tasks = NULL;
    os_task_t *current_task = NULL;
    
    all_tasks = (os_threadpool_t *)args;
    if (!all_tasks->should_stop) {
        do {
            current_task = get_task(all_tasks);
            if (!current_task) {
                usleep(WAIT_TIME);   
            } else {
                current_task->task(current_task->argument);
                free(current_task);
            }
        } while (!all_tasks->should_stop);
    }

    return NULL;
}


void threadpool_stop(os_threadpool_t *tp, int (*processingIsDone)(os_threadpool_t *)) {

    pthread_t *current_thread = NULL;
    if (!tp->num_threads) {
        return;
    }

    tp->should_stop = TRUE;

    current_thread = tp->threads;
    short n = tp->num_threads;
    while (n) {
        size_t status = 0;
        status = pthread_join(*current_thread, NULL);
        if (status) {
           ERROR("Pthread join failed"); 
        }
        --n;
        ++current_thread;
    }
}
