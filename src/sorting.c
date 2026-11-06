/* General functions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* System calls */
#include <unistd.h>
#include <sys/types.h>

/* Named pipe */
#include <sys/stat.h>
#include <fcntl.h>

/* Signals */
#include <signal.h>

/* Threads */
#include <pthread.h>

/* Project files */
#include "common.h"

/* We store the anonymous pipes globally (so that every thread can access them) */
int pipeToRouting[2];
int pipeToArea[AREA_COUNT][2];

void *routingRoutine(void *arg)
{
    /* TODO: read pipeToRouting[0] (the anonymous pipe shared with the 'main' thread), validate, write to pipeToArea[...] */
    return NULL;
}

void *areaRoutine(void *arg)
{
    AreaId area = *(AreaId *)arg;
    printf("Thread Area %c started\n", 'A' + area);
    /* TODO: read pipeToArea[area][0] (the anonymous pipe shared with the 'routing' thread)*/
    return NULL;
}

int main(int argc, char *argv[])
{
    /* We create the pipes to the different threads */
    if(pipe(pipeToRouting) == -1)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    for(int i = 0; i < AREA_COUNT; i++)
    {
        if(pipe(pipeToArea[i]) == -1)
        {
            perror("pipe");
            return EXIT_FAILURE;
        }
    }


    /* We create the threads */
    pthread_t routingThread;
    pthread_t areaThreads[AREA_COUNT];
    AreaId areaIds[AREA_COUNT] = {AREA_A, AREA_B, AREA_C};

    int ret = pthread_create(&routingThread, NULL, routingRoutine, NULL);
    if(ret != 0)
    {
        fprintf(stderr, "pthread_create: %s\n", strerror(ret));
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < AREA_COUNT; i++)
    {
        ret = pthread_create(&areaThreads[i], NULL, areaRoutine, &areaIds[i]);
        if(ret != 0)
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(ret));
            exit(EXIT_FAILURE);
        }
    }

    /* TODO: read the FIFO_MAIN_TO_SORTING pipe (the named pipe shared with the Main process) and forward commands */

    /* We wait for the threads */
    pthread_join(routingThread, NULL);
    for (int i = 0; i < AREA_COUNT; i++)
    {
        pthread_join(areaThreads[i], NULL);
    }

    return EXIT_SUCCESS;
}