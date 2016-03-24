#include "/c/cs323/Hwk5/process-stub.h"
#include <assert.h>

#define TRUE (1)
#define FALSE (0)
#define STATUS_DIGITS (128) // max digits for an exit status

// Print error message and die with EXIT_FAILURE
#define errorExit(reason) perror(reason), exit(errno)

int processInternal(CMD *cmdList, int bg);


void reapZombies(int sig)
{
    pid_t pid;          // child process pid
    int status;         // child process exit status
    while ((pid = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) {
        status = WIFEXITED(status) ?
                 WEXITSTATUS(status) : 128+WTERMSIG(status);
        fprintf(stderr, "Completed: %d (%d)\n", pid, status);
    }
    return;
}

void setVars(CMD *cmdList)
{
    for (int i = 0; i < cmdList->nLocal; i++)
        setenv(cmdList->locVar[i], cmdList->locVal[i], 1);
}


int reportStatus(int status)
{
    char value[STATUS_DIGITS];      // convert status to string
    sprintf(value, "%d", status);
    setenv("?", value, 1);          // set $? to status
    return status;
}

void redirect(CMD *cmdList)
{
    int inFile, outFile;            // read and write file descriptors

    if (cmdList->fromType == NONE && cmdList->fromFile == NULL)
	;
    else if (cmdList->fromType == RED_IN && cmdList->fromFile != NULL) {
        if ((inFile = open(cmdList->fromFile, O_RDONLY)) < 0)       // error
            errorExit(cmdList->fromFile);
        else
            dup2(inFile, 0);
    }

    if (cmdList->toType == NONE && cmdList->toFile == NULL)
        ;
    else if (cmdList->toType == RED_OUT && cmdList->toFile != NULL) {
        if ((outFile = open(cmdList->toFile, O_WRONLY | O_TRUNC | O_CREAT,
                           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH))
            < 0)        // error
            errorExit(cmdList->toFile);
        else
            dup2(outFile, 1);
    } else if (cmdList->toType == RED_OUT_APP && cmdList->toFile != NULL) {
        if ((outFile = open(cmdList->toFile, O_WRONLY | O_APPEND | O_CREAT,
                           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH))
            < 0)        // error
            errorExit(cmdList->toFile);
        else
            dup2(outFile, 1);
    }

}


int simpleCMD(CMD *cmdList, int bg)
{
    if (strcmp(cmdList->argv[0], "cd") == 0 && !bg) {
        char *path;

        setVars(cmdList);
        redirect(cmdList);

        if (cmdList->argc == 1) {
            path = getenv("HOME");
        } else if (cmdList->argc == 2) {
            path = cmdList->argv[1];
        } else {
            fprintf(stderr, "usage: cd OR cd <directory-name>\n");
            return reportStatus(EXIT_FAILURE);
        }

        if (chdir(path) == -1) {
            perror("cd");
            return reportStatus(errno);
        } else 
            return reportStatus(EXIT_SUCCESS);
    } else if (strcmp(cmdList->argv[0], "dirs") == 0 && !bg) {
        if (cmdList->argc != 1) {
            fprintf(stderr, "usage: dirs\n");
            return reportStatus(EXIT_FAILURE);
        }

        char *path = malloc(sizeof(char) * PATH_MAX);

        if (getcwd(path, PATH_MAX) == NULL) {
            perror("dirs");
            free(path);
            return reportStatus(errno);
        } else {
            printf("%s\n", path);
            free(path);
            return reportStatus(EXIT_SUCCESS);
        }
    } else if (strcmp(cmdList->argv[0], "wait") == 0 && !bg) {
        if (cmdList->argc != 1) {
            fprintf(stderr, "usage: wait\n");
            return reportStatus(EXIT_FAILURE);
        }
        
        while (waitpid((pid_t)(-1), NULL, 0) > 0);
        return reportStatus(EXIT_SUCCESS);

    } else {
        pid_t pid = fork();
        int status;

        if (pid < 0) {                               // fork error
            perror(cmdList->argv[0]);
            return reportStatus(errno);
        }


        else if (pid == 0) {                         // child process
            setVars(cmdList);
            redirect(cmdList);
            if (strcmp(cmdList->argv[0], "cd") == 0) {
                char *path;

                if (cmdList->argc == 1)
                    path = getenv("HOME");
                else if (cmdList->argc == 2)
                    path = cmdList->argv[1];
                else {
                    fprintf(stderr, "usage: cd OR cd <directory-name>\n");
                    exit(EXIT_FAILURE);
                }

                if (chdir(path) == -1)
                    errorExit("cd");
                else
                    exit(EXIT_SUCCESS);
            } else if (strcmp(cmdList->argv[0], "dirs") == 0) {
                if (cmdList->argc != 1) {
                    fprintf(stderr, "usage: dirs\n");
                    exit(EXIT_FAILURE);
                }

                char *path = malloc(sizeof(char) * PATH_MAX);

                if (getcwd(path, PATH_MAX) == NULL) {
                    free(path);
                    errorExit("dirs");
                } else {
                    printf("%s\n", path);
                    free(path);
                    exit(EXIT_SUCCESS);
                }
            } else {
                execvp(cmdList->argv[0], cmdList->argv);
                errorExit(cmdList->argv[0]);             // execvp returned, error
            }
        } else {                                     // parent process
            if (bg) {
                fprintf(stderr, "Backgrounded: %d\n", pid);
                status = 0;
            } else {
                waitpid(pid, &status, 0);
                status = WIFEXITED(status) ?
                         WEXITSTATUS(status) : 128+WTERMSIG(status);
            }
        }

        return reportStatus(status);
    }
}


int subCMD(CMD *cmdList, int bg)
{
    pid_t pid = fork();
    int status;

    if (pid < 0) {                      // fork error
        perror("subcommand");
        return reportStatus(errno);
    }

    else if (pid == 0) {                // child process
        redirect(cmdList);
        exit(processInternal(cmdList->left, FALSE));
    } else {                            // parent process
        if (bg) {
            fprintf(stderr, "Backgrounded: %d\n", pid);
            status = 0;
        } else {
            signal(SIGCHLD, reapZombies);
            waitpid(pid, &status, 0);
            status = WIFEXITED(status) ?
                     WEXITSTATUS(status):128+WTERMSIG(status);
        }
    }

    return reportStatus(status);
}

int pipeCMD(CMD *cmdList, int bg)
{
    int args = 0;                  // number of commands in chain
    CMD *itr;                      // iterator
    for (itr = cmdList; itr->type == PIPE; itr = itr->left)      // find # args
        args++;
    args++;

    pid_t pid,                     // current child process ID
          table[args];             // table of PID's for each child

    int fd[2],                     // read and write file descriptors for pipe
        status,                    // current child status
        fdIn;                      // read end of last pipe (or original stdin)

    CMD *commands[args];           // store commands in order of execution
    int index = args - 1;          // index into commands array
    for(itr = cmdList; itr->type == PIPE; itr = itr->left) {
        commands[index] = itr->right;
        index--;
    }
    commands[index] = itr;

    fdIn = 0;       // remember original stdin
    for(int i = 0; i < args-1; i++) {     // create chain of processes
        if (pipe(fd) || (pid = fork()) < 0) {       // TODO: error
            perror("pipe");
            return reportStatus(errno);
        }

        else if (pid == 0) {        // child process
            close(fd[0]);           // no reading from new pipe
            if (fdIn != 0) {        // stdin = read[last pipe]
                dup2(fdIn, 0);
                close(fdIn);
            }
            if (fd[1] != 1) {       // stdout = write[new pipe]
                dup2 (fd[1], 1);
                close (fd[1]);
            }

            redirect(commands[i]);                    // execute ith command
            if (commands[i]->type == SIMPLE) {
                execvp(commands[i]->argv[0], commands[i]->argv);
                errorExit(commands[i]->argv[0]);      // execvp returned, error
            } else {                                  // subcommand
                exit(processInternal(commands[i]->left, bg));
            }
        } else {                    // parent process
            table[i] = pid;         // save child pid
            if (i > 0)              // close read[last pipe]
                close(fdIn);
            fdIn = fd[0];
            close(fd[1]);
        }
    }

    if ((pid = fork()) < 0) {       // create last process
        perror("pipe");             // pipe error
        return reportStatus(errno);
    }

    else if (pid == 0) {            // child process
        if (fdIn != 0) {            // stdin = read[last pipe]
            dup2(fdIn, 0);
            close(fdIn);
        }
        redirect(commands[args-1]); // execute ith command
        if (commands[args-1]->type == SIMPLE) {
            execvp(commands[args-1]->argv[0], commands[args-1]->argv);
        } else {                    // subcommand
            exit(processInternal(commands[args-1]->left, bg));
        }
    } else {                        // parent process
        table[args-1] = pid;        // save child pid
        close(fdIn);                // close read[last pipe]
    }

    int finalStatus = EXIT_SUCCESS;
    for (int i = 0; i < args; ) {   // wait for children to die
        pid = waitpid((pid_t)(-1), &status, WNOHANG);
        int j;
        for (j = 0; j < args && table[j] != pid; j++)
            ;
        if (j < args) {             // ignore zombie processes
            if (status != EXIT_SUCCESS) // child failed
                finalStatus = status;   // save error status
            i++;
        }
    }

    finalStatus = WIFEXITED(finalStatus) ? WEXITSTATUS(finalStatus) :
                                           128+WTERMSIG(finalStatus);

    return reportStatus(finalStatus);
}


int andCMD(CMD *cmdList, int bg)
{
    int status;

    if (bg) {
        pid_t pid = fork();

        if (pid < 0) {                               // fork error
            perror(cmdList->argv[0]);
            return reportStatus(errno);
        }


        else if (pid == 0) {                         // child process
            if (processInternal(cmdList->left, FALSE) == EXIT_SUCCESS)
                processInternal(cmdList->right, FALSE);
            exit(EXIT_SUCCESS);
        } else {                                     // parent process
            fprintf(stderr, "Backgrounded: %d\n", pid);
            status = 0;
        }

        return reportStatus(status);
    } else {
        if ((status = processInternal(cmdList->left, bg)) == EXIT_SUCCESS)
            return processInternal(cmdList->right, bg);
        else
            return status;
    }
}

int orCMD(CMD *cmdList, int bg)
{
    int status;

    if (bg) {
        pid_t pid = fork();

        if (pid < 0) {                               // fork error
            perror(cmdList->argv[0]);
            return reportStatus(errno);
        }


        else if (pid == 0) {                         // child process
            if (processInternal(cmdList->left, FALSE) != EXIT_SUCCESS)
                processInternal(cmdList->right, FALSE);
            exit(EXIT_SUCCESS);
        } else {                                     // parent process
            fprintf(stderr, "Backgrounded: %d\n", pid);
            status = 0;
        }

        return reportStatus(status);
    } else {
        return processInternal(cmdList->left, bg) == EXIT_SUCCESS ?
               EXIT_SUCCESS : processInternal(cmdList->right, bg);
    }
}


void bgCMD(CMD *cmdList, int root)
{
    if (root) {
        if (cmdList->left->type == SEP_BG) {
            bgCMD(cmdList->left, FALSE);

            if (cmdList->right != NULL)
                processInternal(cmdList->right, FALSE);
        } else {
            processInternal(cmdList->left, TRUE);

            if (cmdList->right != NULL)
                processInternal(cmdList->right, FALSE);
        }
    } else {
        processInternal(cmdList->left, TRUE);
        processInternal(cmdList->right, TRUE);
    }
    return;
}


int processInternal(CMD *cmdList, int bg)
{
    if (cmdList->type == SIMPLE) {
        return simpleCMD(cmdList, bg);
    } else if (cmdList->type == SUBCMD) {
        return subCMD(cmdList, bg);
    } else if (cmdList->type == PIPE) {
        return pipeCMD(cmdList, bg);
    } else if (cmdList->type == SEP_AND) {
        return andCMD(cmdList, bg);
    } else if (cmdList->type == SEP_OR) {
        return orCMD(cmdList, bg);
    } else if (cmdList->type == SEP_BG) {
        bgCMD(cmdList, TRUE);
        return EXIT_SUCCESS;
    } else if (cmdList->type == SEP_END) {
        if (cmdList->right == NULL) {
            return processInternal(cmdList->left, FALSE);
        } else {
            processInternal(cmdList->left, FALSE);
            return processInternal(cmdList->right, bg);
        }
    }

    return EXIT_SUCCESS;
}

int process(CMD *cmdList)
{
    signal(SIGCHLD, reapZombies);
    processInternal(cmdList, FALSE);
    return EXIT_SUCCESS;
}
