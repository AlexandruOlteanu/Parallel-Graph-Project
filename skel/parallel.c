#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "os_list.h"

#define TRUE 1
#define FALSE 0
#define ARGS_NUMBER 2
#define FILE_MODE "r"
#define NR_THREADS 4

#define ERROR(message) { \
        fprintf(stderr, "An Error has occured with message: %s\n", message); \
        exit(EXIT_FAILURE); \
}


int queued_tasks = 0;
pthread_mutex_t mtx_queued_tasks;
os_graph_t *tasks_graph = NULL;
pthread_cond_t mtx_finished_tasks;
int result = 0;
os_threadpool_t *all_tasks = NULL;
pthread_mutex_t mtx;
pthread_mutex_t mtx_independent_check;

void solveTask(unsigned int nodeIdx) {
    short status = 0;
    os_node_t *node = tasks_graph->nodes[nodeIdx];
    status = pthread_mutex_lock(&mtx);
    if (status) {
        ERROR("Failed to lock mutex");
    }
    result += node->nodeInfo;
    status = pthread_mutex_unlock(&mtx);
    if (status) {
        ERROR("Failed to unlock mutex");
    }
    int i = 0;
    do {
         status = pthread_mutex_lock(&mtx_independent_check);
        if (status) {
            ERROR("Failed to lock mutex");
        }
        if (tasks_graph->visited[node->neighbours[i]] == 0) {
            tasks_graph->visited[node->neighbours[i]] = 1;
            status = pthread_mutex_unlock(&mtx_independent_check);
            if (status) {
                ERROR("Failed to unlock mutex");
            }

            __intptr_t p = node->neighbours[i];
            os_task_t *currentTask = task_create((void *) p, (void (*)(void *))solveTask);
            status = pthread_mutex_lock(&mtx_queued_tasks);
            if (status) {
                ERROR("Failed to lock mutex");
            }
            ++queued_tasks;
            status = pthread_mutex_unlock(&mtx_queued_tasks);
            if (status) {
                ERROR("Failed to unlock mutex");
            }
            add_task_in_queue(all_tasks, currentTask);
        } else {
            status = pthread_mutex_unlock(&mtx_independent_check);
            if (status) {
                ERROR("Failed to unlock mutex");
            }
        }
        ++i;
    } while (i < node->cNeighbours);

    status = pthread_mutex_lock(&mtx_queued_tasks);
    if (status) {
        ERROR("Failed to lock mutex");
    }
    if (--queued_tasks == 0) {
        status = pthread_cond_signal(&mtx_finished_tasks);
        if (status) {
            ERROR("Failed to execute cond signal");
        }
    }
    status = pthread_mutex_unlock(&mtx_queued_tasks);
    if (status) {
        ERROR("Failed to unlock mutex");
    }
}

int main(int argc, char *argv[]) {

    if (argc != ARGS_NUMBER) {
        ERROR("Wrong usage of the program, correct: ./main file_name")
    }

    FILE *file = fopen(argv[1], FILE_MODE);

    if (file == NULL) {
        ERROR("Opening file failed");
    }
    tasks_graph = create_graph_from_file(file);
    if (tasks_graph == NULL) {
        ERROR("Failed to create graph");
    }

    all_tasks = threadpool_create(tasks_graph->nCount, NR_THREADS);
    if (all_tasks == NULL) {
        ERROR("Failed to work with empty tasks");
    }
    short status = 0;
    status = pthread_mutex_init(&mtx, NULL);
    if (status) {
        ERROR("Failed to initiate mutex");
    }

    int i = 0;
    do {

        status = pthread_mutex_lock(&mtx_independent_check);
        if (status) {
            ERROR("Failed to lock mutex");
        }
        if (tasks_graph->visited[i] == FALSE) {
            tasks_graph->visited[i] = TRUE;

            status = pthread_mutex_unlock(&mtx_independent_check);
            if (status) {
                ERROR("Failed to unlock mutex");
            }

            __intptr_t p = i;
            os_task_t *currentTask = task_create((void *) p, (void (*)(void *))solveTask);
            if (currentTask != NULL) { 
                status = pthread_mutex_lock(&mtx_queued_tasks);
                if (status) {
                    ERROR("Failed to lock mutex");
                }
                ++queued_tasks;
                add_task_in_queue(all_tasks, currentTask);
                status = pthread_mutex_unlock(&mtx_queued_tasks);
                if (status) {
                    ERROR("Failed to unlock mutex");
                }
            } else {
                ERROR("Failed to create Task");
            }
        } else {
            status = pthread_mutex_unlock(&mtx_independent_check);
            if (status) {
                ERROR("Failed to unlock mutex");
            }
        }
        ++i;

    } while (i < tasks_graph->nCount);

    status = pthread_mutex_lock(&mtx_queued_tasks);
    if (status) {
        ERROR("Failed to lock mutex");
    }
    do {
        status = pthread_cond_wait(&mtx_finished_tasks, &mtx_queued_tasks);
        if (status) {
            ERROR("Failed on cond wait");
        }
    } while (queued_tasks > 0);
    
    status = pthread_mutex_unlock(&mtx_queued_tasks);
    if (status) {
        ERROR("Failed to unlock mutex");
    }

    threadpool_stop(all_tasks, NULL);

    const int nr_mutex = 3;
    pthread_mutex_t* all_mutex = malloc(nr_mutex * sizeof(pthread_mutex_t));
    all_mutex[0] = mtx;
    all_mutex[1] = mtx_queued_tasks;
    all_mutex[2] = mtx_independent_check;

    int p = 0;
    do {
        status = pthread_mutex_destroy(&all_mutex[p]);
        if (status) {
            ERROR("Failed on destroing mutex");
        }
        ++p;
    } while (p < nr_mutex);
    
    status = pthread_cond_destroy(&mtx_finished_tasks);
    if (status) {
        ERROR("Failed to destroy cond mutex");
    }

    printf("%d\n", result);
    return 0;
}

