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

/* Fork */
#include <sys/wait.h>

/* Project files */
#include "common.h"

/* We store the anonymous pipes globally */
int pipeToRouting[2];
int pipeToArea[AREA_COUNT][2];
int pipeToSupervision[2];

/* We store the 'main' thread id, so that supervision can signal it */
pthread_t mainThread;

/* We store the target notification */
volatile sig_atomic_t targetReached = 0;


/* We raise the target flag when the signal arrives */
void handleTarget(int signal)
{
    targetReached = 1;
}


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


/* We validate packages and send them to their area */
void *routingRoutine(void *arg)
{
    char line[1024];

    /* We read commands until the 'main' thread closes the pipe */
    while (read(pipeToRouting[0], line, sizeof(line)) > 0)
    {
        /* We extract the package fields */
        Package package;
        char charDestination;

        /* sscanf can't read an enum, we read a char and convert it after */
        int nbExtracted = sscanf(line, "COLIS %u %c %f", &package.packageId, &charDestination, &package.packageWeight);

        /* We reject a bad format or weight */
        if (nbExtracted != 3 || package.packageWeight <= 0)
        {
            fprintf(stderr, "Invalid package format or weight : %s", line);
            continue;
        }

        /* We reject an unknown area */
        AreaId destination = letterToArea(charDestination);
        if (destination == AREA_COUNT)
        {
            fprintf(stderr, "Invalid destination : %s", line);
            continue;
        }

        /* We send the package to its area */
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


/* We handle the packages of one area */
void *areaRoutine(void *arg)
{
    AreaId areaId = *(AreaId *)arg;

    /* We declare the area that corresponds to the thread */
    Area area = {areaId, 0, 0.0f};

    Package package;

    /* We read packages until the 'routing' thread closes the pipe */
    while (read(pipeToArea[areaId][0], &package, sizeof(package)) > 0)
    {
        /* We update the area stats */
        area.totalPackages++;
        area.totalWeight += package.packageWeight;
        printf("Area %c: package %u received (%.2f kg), total %d packages, %.2f kg\n", 'A' + areaId, package.packageId, package.packageWeight, area.totalPackages, area.totalWeight);

        /* We send the package to the supervision thread */
        write(pipeToSupervision[1], &package, sizeof(package));
    }

    return NULL;
}


/* We keep the global stats, send them to display and check the target */
void *supervisionRoutine(void *arg)
{
    /* We create the named pipe to display */
    if (mkfifo(FIFO_SORTING_TO_DISPLAY, 0600) == -1)
    {
        /* We ignore the error if the pipe already exists */
        if (errno != EEXIST)
        {
            perror("mkfifo");
            return NULL;
        }
    }

    /* We open the named pipe for writing */
    int fifoFd = open(FIFO_SORTING_TO_DISPLAY, O_WRONLY);
    if (fifoFd == -1)
    {
        perror("open");
        return NULL;
    }

    Package package;
    CenterStatistics centerStatistics = {0};

    /* We read packages until the 'main' thread closes the pipe */
    while (read(pipeToSupervision[0], &package, sizeof(package)) > 0)
    {
        /* We count the package */
        centerStatistics.numberOfPackageProcessed += 1;

        /* We add its weight */
        centerStatistics.totalWeight += package.packageWeight;

        /* We keep it as the last package */
        centerStatistics.lastPackageReceived = package;

        /* We update the stats of its area */
        centerStatistics.areas[package.packageDestination].totalPackages += 1;
        centerStatistics.areas[package.packageDestination].totalWeight += package.packageWeight;

        /* We send the current state to display */
        write(fifoFd, &centerStatistics, sizeof(centerStatistics));

        /* We warn the 'main' thread when the target is reached */
        if (centerStatistics.numberOfPackageProcessed == TARGET_PACKAGES)
        {
            int ret = pthread_kill(mainThread, SIGUSR1);
            if (ret != 0)
            {
                fprintf(stderr, "pthread_kill: %s\n", strerror(ret));
                return NULL;
            }
        }
    }

    /* We close the named pipe, so that display stops */
    close(fifoFd);
    return NULL;
}


int main(int argc, char *argv[])
{
    /* We make a child to launch the display process */
    pid_t displayPid = fork();

    if(displayPid == -1)
    {
        perror("fork");
        return EXIT_FAILURE;
    }

    /* In the child, we replace the program with display */
    if(displayPid == 0)
    {
        char *args[] = {"./bin/display", NULL};
        execvp(args[0], args);

        /* If we reach this line, execvp failed */
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    /* We store the id of the 'main' thread */
    mainThread = pthread_self();

    /* We prepare the target handler */
    struct sigaction targetAction;
    sigemptyset(&targetAction.sa_mask);
    targetAction.sa_flags = 0;
    targetAction.sa_handler = handleTarget;

    /* We link the handler to SIGUSR1, before creating the threads */
    if (sigaction(SIGUSR1, &targetAction, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /* We create the named pipe from main */
    if (mkfifo(FIFO_MAIN_TO_SORTING, 0600) == -1)
    {
        /* We ignore the error if the pipe already exists */
        if (errno != EEXIST)
        {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }

    /* We create the pipe to the routing thread */
    if (pipe(pipeToRouting) == -1)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    /* We create one pipe per area */
    for (int i = 0; i < AREA_COUNT; i++)
    {
        if (pipe(pipeToArea[i]) == -1)
        {
            perror("pipe");
            return EXIT_FAILURE;
        }
    }

    /* We create the pipe to the supervision thread */
    if (pipe(pipeToSupervision) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* We declare the threads */
    pthread_t routingThread;
    pthread_t supervisionThread;
    pthread_t areaThreads[AREA_COUNT];
    AreaId areaIds[AREA_COUNT] = {AREA_A, AREA_B, AREA_C};

    /* We create the routing thread */
    int ret = pthread_create(&routingThread, NULL, routingRoutine, NULL);
    if (ret != 0)
    {
        fprintf(stderr, "pthread_create: %s\n", strerror(ret));
        return EXIT_FAILURE;
    }

    /* We create the supervision thread */
    ret = pthread_create(&supervisionThread, NULL, supervisionRoutine, NULL);
    if (ret != 0)
    {
        fprintf(stderr, "pthread_create: %s\n", strerror(ret));
        return EXIT_FAILURE;
    }

    /* We create the area threads */
    for (int i = 0; i < AREA_COUNT; i++)
    {
        ret = pthread_create(&areaThreads[i], NULL, areaRoutine, &areaIds[i]);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(ret));
            return EXIT_FAILURE;
        }
    }

    /* We open the named pipe for reading */
    int fifoFd = open(FIFO_MAIN_TO_SORTING, O_RDONLY);
    if (fifoFd == -1)
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

    /* We read commands until the end, QUIT or the target */
    char line[1024];
    while (!targetReached && fgets(line, sizeof(line), fifo) != NULL)
    {
        /* We forward packages to the routing thread, as fixed-size blocks */
        if (strncmp(line, "COLIS ", 6) == 0)
        {
            write(pipeToRouting[1], line, sizeof(line));
        }

        /* We stop on QUIT */
        else if (strcmp(line, "QUIT\n") == 0)
        {
            /* We stop display later, once supervision is done (to avoid SIGPIPE) */
            break;
        }

        /* We handle the state request */
        else if (strcmp(line, "ETAT\n") == 0)
        {
            /* TODO: ask the supervision thread for the current state */
        }
    }

    /* We stop the main process if the target was reached */
    if (targetReached)
    {
        kill(getppid(), SIGTERM);
    }

    /* We close the routing pipe to stop the routing thread */
    close(pipeToRouting[1]);

    /* We close the named pipe */
    fclose(fifo);

    /* We wait for the routing and area threads */
    pthread_join(routingThread, NULL);
    for (int i = 0; i < AREA_COUNT; i++)
    {
        pthread_join(areaThreads[i], NULL);
    }

    /* We close the supervision pipe once no area can write anymore */
    close(pipeToSupervision[1]);

    /* We wait for the supervision thread */
    pthread_join(supervisionThread, NULL);

    /* We send a signal to stop the display process */
    kill(displayPid, SIGTERM);

    /* We wait for the display process */
    int status = 0;
    if (waitpid(displayPid, &status, 0) == -1)
    {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    if(WIFEXITED(status))
    {
        printf("Display (pid : %d) finished normally, code = %d\n",
            displayPid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        printf("Display (pid %d) killed by signal %d\n", displayPid, WTERMSIG(status));
    }

    /* We remove the named pipe to display */
    if (unlink(FIFO_SORTING_TO_DISPLAY) == -1)
    {
        perror("unlink");
    }

    return EXIT_SUCCESS;
}