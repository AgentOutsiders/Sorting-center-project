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

/* Project files */
#include "common.h"

/* We store the stop request */
volatile sig_atomic_t stopRequested = 0;

/* We raise the stop flag when the signal arrives */
void handleStop(int signal)
{
    stopRequested = 1;
}

/* We show the center statistics */
static void showStatistics(const CenterStatistics *stats)
{
    printf("Number of package processed : %d\n", stats->numberOfPackageProcessed);
    printf("Total weight : %.2f\n", stats->totalWeight);
}


int main(int argc, char *argv[])
{
    /* We create the named pipe from sorting */
    if (mkfifo(FIFO_SORTING_TO_DISPLAY, 0600) == -1)
    {
        /* We ignore the error if the pipe already exists */
        if (errno != EEXIST)
        {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }

    /* We prepare the stop handler */
    struct sigaction stopAction;
    sigemptyset(&stopAction.sa_mask);
    stopAction.sa_flags = 0;
    stopAction.sa_handler = handleStop;

    /* We link the handler to SIGTERM */
    if (sigaction(SIGTERM, &stopAction, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }


    /* We open the named pipe for reading */
    int fifoFd = open(FIFO_SORTING_TO_DISPLAY, O_RDONLY);
    if (fifoFd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* We read the center state until sorting closes the pipe */
    CenterStatistics centerStatistics;
    while (!stopRequested && read(fifoFd, &centerStatistics, sizeof(centerStatistics)) > 0)
    {
        /* We show the global stats */
        showStatistics(&centerStatistics);
    }
    
    /* We empty the pipe after SIGTERM */
    while (read(fifoFd, &centerStatistics, sizeof(centerStatistics)) > 0)
    {
        /* We show the states that were still waiting in the pipe */
        showStatistics(&centerStatistics);
    }

    /* We close the named pipe */
    close(fifoFd);
    return EXIT_SUCCESS;
}