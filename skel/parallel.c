#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "os_list.h"

#define MAX_TASK 100
#define MAX_THREAD 4

int sum = 0;
os_graph_t *graph;
os_threadpool_t *tp;
pthread_mutex_t sum_mutex; // Mutex for sum
int pending_tasks = 0; // Counter for pending tasks
pthread_mutex_t pending_tasks_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for pending tasks counter
pthread_cond_t tasks_done_cond = PTHREAD_COND_INITIALIZER; // Conditional variable for tasks done
pthread_mutex_t visited_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for visited array

void processNode(unsigned int nodeIdx)
{
    os_node_t *node = graph->nodes[nodeIdx];
    pthread_mutex_lock(&sum_mutex); // Lock the mutex
    sum += node->nodeInfo;
    pthread_mutex_unlock(&sum_mutex); // Unlock the mutex
    for (int i = 0; i < node->cNeighbours; i++) {
        pthread_mutex_lock(&visited_mutex);
        if (graph->visited[node->neighbours[i]] == 0) {
            graph->visited[node->neighbours[i]] = 1;
            pthread_mutex_unlock(&visited_mutex);

            os_task_t *task = task_create((void*)(intptr_t)node->neighbours[i], (void (*)(void *))processNode);
            pthread_mutex_lock(&pending_tasks_mutex);
            pending_tasks++;
            pthread_mutex_unlock(&pending_tasks_mutex);
            add_task_in_queue(tp, task);
        } else {
            pthread_mutex_unlock(&visited_mutex);
        }
    }

    pthread_mutex_lock(&pending_tasks_mutex);
    pending_tasks--;
    if (pending_tasks == 0) {
        pthread_cond_signal(&tasks_done_cond);
    }
    pthread_mutex_unlock(&pending_tasks_mutex);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: ./main input_file\n");
        exit(1);
    }

    FILE *input_file = fopen(argv[1], "r");

    if (input_file == NULL) {
        printf("[Error] Can't open file\n");
        return -1;
    }

    graph = create_graph_from_file(input_file);
    if (graph == NULL) {
        printf("[Error] Can't read the graph from file\n");
        return -1;
    }

    // Create threadpool with desired number of threads
    tp = threadpool_create(graph->nCount, MAX_THREAD);

    // Initialize the mutex for sum
    pthread_mutex_init(&sum_mutex, NULL);

    for (int i = 0; i < graph->nCount; i++)
    {
        pthread_mutex_lock(&visited_mutex);
        if (graph->visited[i] == 0) {
            graph->visited[i] = 1;
            pthread_mutex_unlock(&visited_mutex);

            os_task_t *task = task_create((void*)(intptr_t)i, (void (*)(void *))processNode);
            pthread_mutex_lock(&pending_tasks_mutex);
            pending_tasks++;
            pthread_mutex_unlock(&pending_tasks_mutex);
            add_task_in_queue(tp, task);
        } else {
            pthread_mutex_unlock(&visited_mutex);
        }
    }

    pthread_mutex_lock(&pending_tasks_mutex);
    while (pending_tasks > 0) {
        pthread_cond_wait(&tasks_done_cond, &pending_tasks_mutex);
    }
        pthread_mutex_unlock(&pending_tasks_mutex);

    // Wait for all tasks to finish
    threadpool_stop(tp, NULL);

    // Destroy the mutex for sum
    pthread_mutex_destroy(&sum_mutex);

    // Destroy the mutex and conditional variable for pending tasks
    pthread_mutex_destroy(&pending_tasks_mutex);
    pthread_cond_destroy(&tasks_done_cond);

    // Destroy the mutex for visited array
    pthread_mutex_destroy(&visited_mutex);

    printf("%d", sum);
    return 0;
}

