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

    /* We open the named pipe for reading */
    int fifoFd = open(FIFO_SORTING_TO_DISPLAY, O_RDONLY);
    if (fifoFd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* We read the center state until sorting closes the pipe */
    CenterStatistics centerStatistics;
    while (read(fifoFd, &centerStatistics, sizeof(centerStatistics)) > 0)
    {
        /* We show the global stats */
        printf("Number of package processed : %d\n", centerStatistics.numberOfPackageProcessed);
        printf("Total weight : %.2f\n", centerStatistics.totalWeight);
    }

    /* We close the named pipe */
    close(fifoFd);
    return EXIT_SUCCESS;
}