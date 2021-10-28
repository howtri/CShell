#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

bool allowBackground = true;

/* Our signal handler for SIGINT based off the canvas signals exploration*/
void handle_SIGTSTP_shell(int signo) {
    char* message = "Switching to disallow background procs\n";
    allowBackground = false;
    // We are using write rather than printf
    write(STDOUT_FILENO, message, 40);
}

void handle_SIGTSTP_child(int signo) {
    char* message = "Caught SIGTSTP, now go f off\n";
    // We are using write rather than printf
    write(STDOUT_FILENO, message, 40);
    sleep(2);
}

struct shellAttributes
{
    struct bgProcesss* bgActive;
    // set to 0 at the start
    int lastForegroundStatus;
};

struct bgProcess
{
    int pid;
    struct bgProcess* next;
};

struct command 
{
    char* binary;
    // continually reallocate for args?
    char** arguments;
    int status;
    bool background;
};

void exitShell()
{
    // kill any other procs or jobs started
}

char* charExpansion(char* expand)
{
    int count = 0;
    int countExpanded = 0;
    char* expanded = calloc(strlen(expand), sizeof(char));
    bool previousMatch = false;
    while (expand[count] != '\0')
    {
        if (expand[count] == '$')
        {
            if (previousMatch) {
                char* pid;
                int size = asprintf(&pid, "%d", getpid());
                strcat(expanded, pid);
                // reallocate memory to account for expanding $$ to the pid
                expanded = realloc(expanded, strlen(expanded) + size - 1);
                countExpanded += size;
                free(pid);
                previousMatch = false;
            }
            else
            {
                // see $ for the first time. wait to append this char until we see if the next is also $
                previousMatch = true;
            }
            // always increment count but hold countExpanded when we've just seen one $
            count++;
            continue;
        }
        if (previousMatch) {
            // the previous was $ but it wasn't followed by another $. go back and add it
            expanded[countExpanded] = '$';
            countExpanded++;
            previousMatch = false;
        }
        expanded[countExpanded] = expand[count];
        countExpanded++;
        count++;
    }

    // if a single $ was the last character add outside the loop
    if (previousMatch) {
        expanded[countExpanded] = '$';
    }
    return expanded;
}

bool parseCommand(char* input, struct command* currCommand)
{
    char* saveptr = input;
    char* token;
    
    bool firstProcessed = false;
    int argCounter = 0;

    while ((token = strtok_r(saveptr, " \n", &saveptr)))
    {
        // perform character expansion and set token to returned char array pointer
        token = charExpansion(token);
        // check for comments and pull the binary
        if (!firstProcessed)
        {
            firstProcessed = true;
            // skip over any comments
            if (strncmp("#", token, 1) == 0)
            {
                return false;
            }
            // store our binary name
            currCommand->binary = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currCommand->binary, token);
            free(token);

            // store binary name as the first index of arguments
            // initialize for a max of 512 arguments with space for a null terminator
            //
            // IMPROVEMENT
            // GET REALLOC TO WORK!!!!
            //
            //
            currCommand->arguments = malloc(513 * sizeof(char*));
            currCommand->arguments[0] = malloc(strlen(currCommand->binary) + 1);
            strcpy(currCommand->arguments[0], currCommand->binary);
            //currCommand->arguments[0] = currCommand->binary;
            argCounter++;
            continue;
        }
        

        currCommand->arguments[argCounter] = malloc(strlen(token) + 1);
        //currCommand->arguments[argCounter] = token;
        strcpy(currCommand->arguments[argCounter], token);
        argCounter++;
    }
    currCommand->arguments[argCounter] = '\0';

    // check if its a background process
    currCommand->background = false;
    char* lastVal = currCommand->arguments[argCounter - 1];
    if (strcmp(lastVal, "&") == 0)
    {
        // if we've been set with SIGTSTP to disallow background procs
        if (allowBackground)
        {
            currCommand->background = true;
        }
        // we dont want to pass & as an arg so we move \0 to &s position
        currCommand->arguments[argCounter - 1] = '\0';
        free(currCommand->arguments[argCounter]);
    }
    return true;
}

bool handleBuiltIns(struct shellAttributes* currShell, struct command* current)
{
    // changes directory based on argument of relative or absolute path
    // if no path is provided change to the HOME directory
    if (strcmp(current->binary, "cd") == 0)
    {
        // change to HOME directory if the only arg is cd
        if (current->arguments[1] == NULL) 
        {
            char* homeDir = getenv("HOME");       

            // REMOVE --- for debug only
            // char* workingDir[100];       - -------------------------------------------------------------------------------- !
            //printf("%s\n", getcwd(workingDir, 100));
            //
            //

            chdir(homeDir);
        } 
        else // DO WE WANT TO CHECK THAT ITS EXACTLY 1?
        {
            chdir(current->arguments[1]);
        }
        return true;
    }
    else if (strcmp(current->binary, "status") == 0)
    {
        printf("exit status %d\n", currShell->lastForegroundStatus);
        fflush(stdout);
        printf("I need to have support for signals also\n");
        return true;
    }
    else if (strcmp(current->binary, "exit") == 0)
    {
        printf("exiting\n"); // NEED TO CHECK NO ARGS?
        fflush(stdout);
        printf("I NEED TO KILL ALL PROCS I STARTED\n");
        exit(0);
    }
    return false;
}

bool wireIORedirection(struct command* currCommand)
{
    // The redirection must be done before using exec() to run the command but should be done in the child proc.
    
    // create an entire new array so we don't have to worry about shifting values upon detecting io redirection

    char **ioArguments = malloc(513 * sizeof(char*));
    bool wiredStdout = false;
    bool wiredStdin = false;
    // copy over all arguments until chars < or > are encountered
    int ioCounter = 0;
    int argCounter = 0;
    while (currCommand->arguments[argCounter] != '\0')
    {
        if (strcmp(currCommand->arguments[argCounter], "<") == 0)
        {
            // An input file redirected via stdin should be opened for reading only; if your shell cannot open the file for reading, it should print an error message and set the exit status to 1 (but don't exit the shell).
            char* inFilePath = currCommand->arguments[argCounter + 1];
            int fd = open(inFilePath, O_RDONLY);
            if (fd == -1) 
            {
                printf("error: open failed for reading \"%s\"\n", inFilePath);
                fflush(stdout);
                return false;
            }
            // Use dup2 to point in file to standard in - based off canvas example for io
            int result = dup2(fd, 0);
            if (result == -1) 
            {
                perror("dup2");
                fflush(stderr);
            }

            wiredStdin = true;
            argCounter += 2;
        }
        else if (strcmp(currCommand->arguments[argCounter], ">") == 0)
        {
            // Similarly, an output file redirected via stdout should be opened for writing only; it should be truncated if it already exists or created if it does not exist. If your shell cannot open the output file it should print an error message and set the exit status to 1 (but don't exit the shell).
            char* outFilePath = currCommand->arguments[argCounter + 1];
            int fd = open(outFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fd == -1) 
            {
                printf("open failed for writing \"%s\"\n", outFilePath);
                fflush(stdout);
                return false;
            }
            // use dup2 to point stdout to target file - based off canvas example for io
            int result = dup2(fd, 1);
            if (result == -1) {
                perror("dup2()");
                fflush(stderr);
            }

            wiredStdout = true;
            argCounter += 2;
        }
        else
        {
            ioArguments[ioCounter] = malloc(strlen(currCommand->arguments[argCounter]) + 1);
            strcpy(ioArguments[ioCounter], currCommand->arguments[argCounter]);
            ioCounter++;
            argCounter++;
        }
    }
    ioArguments[ioCounter] = '\0';

    
    // wire stdin and out for background processes if not user redirected
    if (currCommand->background) 
    {
        char const devNull[] = "/dev/null";
        if (!wiredStdin)
        {
            int fd = open(devNull, O_RDONLY);
            if (fd == -1)
            {
                printf("error: open failed for reading \"%s\"\n", devNull);
                fflush(stdout);
                return false;
            }
            printf("opened\n");
            // Use dup2 to point in file to standard in - based off canvas example for io
            int result = dup2(fd, 0);
            if (result == -1)
            {
                perror("dup2");
                fflush(stderr);
            }
        }

        if (!wiredStdout)
        {
            int fd = open(devNull, O_WRONLY);
            if (fd == -1)
            {
                printf("open failed for writing \"%s\"\n", devNull);
                fflush(stdout);
                return false;
            }
            printf("opened\n");
            // use dup2 to point stdout to target file - based off canvas example for io
            int result = dup2(fd, 1);
            if (result == -1) {
                perror("dup2()");
                fflush(stderr);
            }
        }
    }
    
    // free our prior args and replace with ioArgs
    while (currCommand->arguments[argCounter] != '\0')
    {
        //  THINK WE MAY BE ABLE TO FREE ONE AFTER THIS TOO FOR OUR NULL TERMINATOR! -----------------------------------------------------------------------------!
        free(currCommand->arguments[argCounter]);
        argCounter++;
    }
    free(currCommand->arguments);

    currCommand->arguments = ioArguments;

    ioCounter = 0;
    printf("Our io arguments sir -----> ");
    while (currCommand->arguments[ioCounter] != '\0')
    {
        printf("%s ", currCommand->arguments[ioCounter]);
        ioCounter++;
    }
    return true;
}

void executeForeground(struct shellAttributes* shell, struct command* current)
{
    // below based on the canvas example in process API - Executing a new program
    int childStatus;

    // Fork a new process
    pid_t spawnPid = fork();

    switch (spawnPid) {
    case -1:
        perror("fork()\n");
        exit(1);
        break;
    case 0:
        // In the child process
        if (!wireIORedirection(current))
        {
            printf("Something terrible! Set the exit status to 1\n");
            fflush(stdout);
            shell->lastForegroundStatus = 1;
            break;
        }

        // set signal for SIGINT for foreground children to default behavior based off canvas signal exploration
        struct sigaction SIGINT_action = { 0 };
        // SET SIGINT to be ignored
        SIGINT_action.sa_handler = SIG_DFL;
        sigfillset(&SIGINT_action.sa_mask);
        SIGINT_action.sa_flags = 0;
        // Install our signal handler
        sigaction(SIGINT, &SIGINT_action, NULL);

        // Initialize SIGTSTP_action struct to be empty - code based off canvas signals exploration
        struct sigaction SIGTSTP_action = { 0 };
        // SET SIGTSTP to use a custom handler
        SIGTSTP_action.sa_handler = handle_SIGTSTP_child;
        sigfillset(&SIGTSTP_action.sa_mask);
        SIGTSTP_action.sa_flags = 0;
        // Install our signal handler
        sigaction(SIGTSTP, &SIGTSTP_action, NULL);

        execvp(current->binary, current->arguments);

        // exec only returns if there is an error
        perror("execvp");

        // exit status of 1 when binary not found
        exit(1);
    default:
        // In the parent process
        // Wait for child's termination
        spawnPid = waitpid(spawnPid, &childStatus, 0);

        if (WIFEXITED(childStatus))
        {
            shell->lastForegroundStatus = WEXITSTATUS(childStatus);
        }
        printf("%d status code\n", shell->lastForegroundStatus);
        fflush(stdout);

        /*
        * 
        } else{
			printf("Child %d exited abnormally due to signal %d\n", childPid, WTERMSIG(childStatus));
		}
        
        */


        printf("PARENT(%d): child(%d) terminated. Exiting\n", getpid(), spawnPid);
        fflush(stdout);
        break;
    }
}

int executeBackground(struct command* current)
{
    // below based on the canvas example in process API - Executing a new program
    // Fork a new process
    pid_t spawnPid = fork();

    switch (spawnPid) {
    case -1:
        perror("fork()\n");
        exit(1);
        break;
    case 0:
        // In the child process
        if (!wireIORedirection(current))
        {
            printf("Something terrible! Set the exit status to 1\n");
            // set this, pass in shell
            // shell->lastForegroundStatus = 2;
        }

        // set signal for SIGTSTP for background children to ignore based off canvas signal exploration
        struct sigaction SIGTSTP_action = { 0 };
        // SET SIGINT to be ignored
        SIGTSTP_action.sa_handler = SIG_IGN;
        sigfillset(&SIGTSTP_action.sa_mask);
        SIGTSTP_action.sa_flags = 0;
        // Install our signal handler
        sigaction(SIGTSTP, &SIGTSTP_action, NULL);

        execvp(current->binary, current->arguments);
        // exec only returns if there is an error
        perror("execvp");
        exit(1);
    default:
        // In the parent process
        // Wait for child's termination
        printf("PARENT(%d) launched BACKGROUND child(%d)\n", getpid(), spawnPid);
        fflush(stdout);
        return spawnPid;
    }

}

void checkBackgroundProcs(struct shellAttributes* shell)
{
    struct bgProcess* iter = NULL;
    struct bgProcess* previous = NULL;
    iter = shell->bgActive;
    while (iter != NULL)
    {
        printf("Looking at bg pid %d\n", iter->pid);
        fflush(stdout);
        int childStatus;
        int childReturn= waitpid(iter->pid, &childStatus, WNOHANG);
        if (childReturn) {
            if (WIFEXITED(childStatus))
            {
                int status = WEXITSTATUS(childStatus);
                printf(" !!!!!!!!!!!!!!! Returned pid %d with status %d\n", childReturn, status);
                fflush(stdout);
                /*
                
                } else{
			printf("Child %d exited abnormally due to signal %d\n", childPid, WTERMSIG(childStatus));
		}

                */



            }
            if (previous != NULL)
            {
                // remove from middle of linked list by setting previous to skip it
                previous->next = iter->next;
                struct bgProcess* hold = iter;
                printf("first free causing issues??\n");
                fflush(stdout);
                iter = iter->next;
                printf("I FREED %p", hold);
                free(hold);
                printf("NOT first free causing issues\n");
                fflush(stdout);

            } 
            else
            {
               // we're at the start so we set the head to the following
                shell->bgActive = iter->next;
                struct bgProcess* hold = iter;
                printf("No its the second!\n");
                fflush(stdout);
                iter = iter->next;
                printf("I FREED %p", hold);
                free(hold);
                printf("NOT second free causing issues\n");
                fflush(stdout);
            }
            // iter is now a new node and previous is directly before our new iter
            continue;
        }
        printf("Not the frees somehow\n");
        previous = iter;
        iter = iter->next;
    }
}

int main(int argc, char* argv[])
{
    // Initialize SIGINT_action struct to be empty - code from canvas signals exploration
    struct sigaction SIGINT_action = { 0 };
    // SET SIGINT to be ignored
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    // Install our signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Initialize SIGTSTP_action struct to be empty - code based off canvas signals exploration
    struct sigaction SIGTSTP_action = { 0 };
    // SET SIGTSTP to use a custom handler
    SIGTSTP_action.sa_handler = handle_SIGTSTP_shell;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    // Install our signal handler
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // initialize our shellAttributes struct to store info the shell as a whole needs
    struct shellAttributes* shell = malloc(sizeof(struct shellAttributes));
    shell->bgActive = NULL;
    shell->lastForegroundStatus = 0;
    bool run = true;
    while (run)
    {
        // while backgroundProc->next != NULL - waitid nhohang for all.
        checkBackgroundProcs(shell);
        printf(": ");
        fflush(stdout);
        char* line = NULL;
        size_t len = 0;
        getline(&line, &len, stdin);

        if (*line == '\n')
        {
            continue;
        }

        // create our struct for the command we'll process
        struct command* currCommand = malloc(sizeof(struct command));
        if (!parseCommand(line, currCommand))
        {
            free(currCommand);
            free(line);
            continue;
        }
        free(line);
        
        if (!handleBuiltIns(shell, currCommand))
        {
            if (currCommand->background)
            {
                struct bgProcess* background = malloc(sizeof(struct bgProcess));
                background->pid = executeBackground(currCommand);
                // if there are no active background procs this becomes the first
                // otherwise add at the front of the list
                if (shell->bgActive == NULL)
                {
                    printf("First background!\n");
                    shell->bgActive = background;
                }
                else
                {
                    background->next = shell->bgActive;
                    shell->bgActive = background;
                }
            }
            else
            {
                // pass shell so we can easily store exit information
                executeForeground(shell, currCommand);
            }
        }
        
        // free dynamically allocated
        free(currCommand->binary);
        int argCounter = 0;
        while (currCommand->arguments[argCounter] != '\0')
        {
            //  THINK WE MAY BE ABLE TO FREE ONE AFTER THIS TOO FOR OUR NULL TERMINATOR! -----------------------------------------------------------------------------!
            free(currCommand->arguments[argCounter]);
            argCounter++;
        }
        free(currCommand->arguments);
        free(currCommand);
    }
    // free shell and all that  !!!!!!!!!!!!!!!!!! -----------------------------------------------------------------------------!
    return EXIT_SUCCESS;
}
