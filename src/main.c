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

/* Fork */
#include <sys/wait.h>

/* Project files */
#include "common.h"

/* We store the stop request */
volatile sig_atomic_t stopRequested = 0;


/* We raise the stop flag when the signal arrives */
void handleStop(int signal)
{
    stopRequested = 1;
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

    /* We make a child to launch the sorting process */
    pid_t sortingPid = fork();

    if(sortingPid == -1)
    {
        perror("fork");
        return EXIT_FAILURE;
    }

    /* In the child, we replace the program with sorting */
    if(sortingPid == 0)
    {
        char *args[] = {"./bin/sorting", NULL};
        execvp(args[0], args);

        /* If we reach this line, execvp failed */
        perror("execvp");
        exit(EXIT_FAILURE);
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

    /* We open the named pipe for writing */
    int fifoFd = open(FIFO_MAIN_TO_SORTING, O_WRONLY);
    if (fifoFd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* We read commands until the end or a signal */
    char line[1024];
    while (!stopRequested && fgets(line, sizeof(line), stdin) != NULL)
    {
        /* We send a package */
        if (strncmp(line, "COLIS ", 6) == 0)
        {
            write(fifoFd, line, strlen(line));
        }

        /* We send a state request */
        else if (strcmp(line, "ETAT\n") == 0)
        {
            write(fifoFd, line, strlen(line));
        }

        /* We send the stop command, then leave */
        else if (strcmp(line, "QUIT\n") == 0)
        {
            write(fifoFd, line, strlen(line));
            break;
        }

        /* We report an unknown command */
        else
        {
            fprintf(stderr, "Unknown command: %s", line);
        }
    }

    /* We close the named pipe */
    close(fifoFd);

    /* We wait for the sorting process */
    int status = 0;
    if (waitpid(sortingPid, &status, 0) == -1)
    {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    if(WIFEXITED(status))
    {
        printf("Sorting (pid : %d) finished normally, code = %d\n",
            sortingPid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        printf("Sorting (pid %d) killed by signal %d\n",
            sortingPid, WTERMSIG(status));
    }

    /* We remove the named pipe to sorting */
    if (unlink(FIFO_MAIN_TO_SORTING) == -1)
    {
        perror("unlink");
    }

    printf("Main (pid %d) finished normally\n", getpid());

    return EXIT_SUCCESS;
}