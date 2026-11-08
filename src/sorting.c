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

/* We convert a destination letter into an area id, or AREA_COUNT if invalid */
static int letterToArea(char letter)
{
    switch (letter)
    {
        case 'A': return AREA_A;
        case 'B': return AREA_B;
        case 'C': return AREA_C;
        default:  return AREA_COUNT;
    }
}

void *routingRoutine(void *arg)
{
    char line[1024];

    /* We read commands until the 'main' thread closes the pipe */
    while(read(pipeToRouting[0], line, sizeof(line)) > 0)
    {
        /* We extract the package fields */
        Package package;
        char charDestination;
        /* sscanf can't read an enum, we store the destination in char type and convert it to AreaId type after */
        int nbExtracted = sscanf(line, "COLIS %u %c %f", &package.packageId, &charDestination, &package.packageWeight);
        
        if(nbExtracted != 3 || package.packageWeight <= 0)
        {
            fprintf(stderr, "Invalid package format or weight : %s", line);
            continue;
        }

        AreaId destination = letterToArea(charDestination);

        if(destination == AREA_COUNT)
        {
            fprintf(stderr, "Invalid destination : %s", line);
            continue;
        }

        package.packageDestination = destination;
        write(pipeToArea[package.packageDestination][1], &package, sizeof(package));
    }

    /* We close the area pipes so that the area threads stop */
    for (int i = 0; i < AREA_COUNT; i++)
    {
        close(pipeToArea[i][1]);
    }

    return NULL;
}

void *areaRoutine(void *arg)
{
    AreaId areaId = *(AreaId *)arg;

    /* We declare the area that corresponds to the thread */
    Area area = {areaId, 0, 0.0f};

    Package package;

    /* We read packages until the 'routing' thread closes the pipe */
    while(read(pipeToArea[areaId][0], &package, sizeof(package)) > 0)
    {
        area.totalPackages ++;
        area.totalWeight += package.packageWeight;
        printf("Area %c: package %u received (%.2f kg), total %d packages, %.2f kg\n",'A' + areaId, package.packageId, package.packageWeight,area.totalPackages, area.totalWeight);
        /* TODO: send the area stats to the supervision thread */
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    /* We create the named pipe to sorting */
    if (mkfifo(FIFO_MAIN_TO_SORTING, 0600) == -1)
    {
        /* We ignore the error if the pipe already exists */
        if (errno != EEXIST)
        {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }
    
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
        return EXIT_FAILURE;
    }

    for (int i = 0; i < AREA_COUNT; i++)
    {
        ret = pthread_create(&areaThreads[i], NULL, areaRoutine, &areaIds[i]);
        if(ret != 0)
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(ret));
            return EXIT_FAILURE;
        }
    }

    /* We open the named pipe for reading */
    int fifoFd = open(FIFO_MAIN_TO_SORTING,O_RDONLY);
    if(fifoFd == -1)
    {
        perror("open O_RDONLY");
        return EXIT_FAILURE;
    }

    /* We turn the descriptor into a FILE* to read line by line */
    FILE *fifo = fdopen(fifoFd, "r");
    if (fifo == NULL)
    {
        perror("fdopen");
        return EXIT_FAILURE;
    }

    /* We read commands until the Main process closes the pipe */
    char line[1024];
    while(fgets(line, sizeof(line), fifo) != NULL)
    {
        /* We forward packages to the routing thread */
        if (strncmp(line, "COLIS ", 6) == 0)
        {
            write(pipeToRouting[1], line, sizeof(line));
        }

        /* We stop on QUIT */
        else if (strcmp(line, "QUIT\n") == 0)
        {
            /* TODO: send a signal to stop the display process */
            break;
        }

        /* We handle the state request */
        else if (strcmp(line, "ETAT\n") == 0)
        {
            /* TODO: ask the supervision thread for the current state */
        }
    }

    /* We close the 'pipeToRouting' anonymous pipe to stop the 'routing' thread */
    close(pipeToRouting[1]);

    /* We close the named pipe */
    fclose(fifo);

    /* We wait for the threads */
    pthread_join(routingThread, NULL);
    for (int i = 0; i < AREA_COUNT; i++)
    {
        pthread_join(areaThreads[i], NULL);
    }

    return EXIT_SUCCESS;
}