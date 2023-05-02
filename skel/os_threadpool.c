#include "os_threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* === TASK === */

/* Creates a task that thread must execute */
os_task_t *task_create(void *arg, void (*f)(void *))
{
    os_task_t *task = (os_task_t *)malloc(sizeof(os_task_t));
    task->task = f;
    task->argument = arg;
    return task;
}

/* Add a new task to threadpool task queue */
void add_task_in_queue(os_threadpool_t *tp, os_task_t *t)
{
    os_task_queue_t *new_node = (os_task_queue_t *)malloc(sizeof(os_task_queue_t));
    new_node->task = t;
    new_node->next = NULL;

    pthread_mutex_lock(&tp->taskLock);
    os_task_queue_t *last_node = tp->tasks;
    
    // The queue is empty, add first node
    if (last_node == NULL) {
        tp->tasks = new_node;
        pthread_mutex_unlock(&tp->taskLock);
        return;
    }

    // The queue is not empty, add last node.
    while (last_node->next) {
        last_node = last_node->next;
    }
    last_node->next = new_node;
    pthread_mutex_unlock(&tp->taskLock);
}

/* Get the head of task queue from threadpool */
os_task_t *get_task(os_threadpool_t *tp)
{
    pthread_mutex_lock(&tp->taskLock);
    // If the queue is empty, return null.
    if (tp->tasks == NULL) {
        pthread_mutex_unlock(&tp->taskLock);
        return NULL;
    }
    // If it's not empty, retrieve the first node
    // And make the queue point to the second node
    os_task_queue_t *task_node = tp->tasks;
    tp->tasks = task_node->next;
    pthread_mutex_unlock(&tp->taskLock);

    // Store the task in a variable and free the node
    os_task_t *task = task_node->task;
    free(task_node);
    return task;
}

/* === THREAD POOL === */

/* Initialize the new threadpool */
os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads)
{
    os_threadpool_t *tp = (os_threadpool_t *)malloc(sizeof(os_threadpool_t));
    tp->should_stop = 0;
    tp->num_threads = nThreads;
    tp->threads = (pthread_t *)malloc(nThreads * sizeof(pthread_t));

    tp->tasks = (os_task_queue_t *)malloc(sizeof(os_task_queue_t));
    tp->tasks->task = NULL;
    tp->tasks->next = NULL;

    pthread_mutex_init(&tp->taskLock, NULL);

    for (int i = 0; i < nThreads; i++) {
        pthread_create(&tp->threads[i], NULL, thread_loop_function, (void *)tp);
    }
    
    return tp;
}

/* Loop function for threads */
void *thread_loop_function(void *args)
{
    os_threadpool_t *tp = (os_threadpool_t *)args;
    while (!tp->should_stop) {
        os_task_t *task = get_task(tp);
        if (task) {
            task->task(task->argument);
            free(task);
        } else {
            usleep(1000);
        }
    }
    return NULL;
}

/* Stop the thread pool once a condition is met */
void threadpool_stop(os_threadpool_t *tp, int (*processingIsDone)(os_threadpool_t *))
{   
    tp->should_stop = 1;

    for (int i = 0; i < tp->num_threads; i++) {
        pthread_join(tp->threads[i], NULL);
    }
}
