#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/types.h>


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
        currCommand->background = true;
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
        printf("I need to have support for signals also\n");
        return true;
    }
    else if (strcmp(current->binary, "exit") == 0)
    {
        printf("exiting\n"); // NEED TO CHECK NO ARGS?
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

    // copy over all arguments until chars < or > are encountered
    int ioCounter = 0;
    int argCounter = 0;
    while (currCommand->arguments[argCounter] != '\0')
    {
        if (strcmp(currCommand->arguments[argCounter], "<") == 0)
        {
            printf("Doing some stuff for input\n");
            // An input file redirected via stdin should be opened for reading only; if your shell cannot open the file for reading, it should print an error message and set the exit status to 1 (but don't exit the shell).
            char* inFilePath = currCommand->arguments[argCounter + 1];
            //int file_descriptor = open(inFilePath, O_RD | O_CREAT | O_TRUNC, 0640);
            // dup2()
            argCounter += 2;

            // if can't open either
            return false;
        }
        else if (strcmp(currCommand->arguments[argCounter], ">") == 0)
        {
            printf("Doing some stuff for output\n");
            // Similarly, an output file redirected via stdout should be opened for writing only; it should be truncated if it already exists or created if it does not exist. If your shell cannot open the output file it should print an error message and set the exit status to 1 (but don't exit the shell).
            char* outFilePath = currCommand->arguments[argCounter + 1];
            //int file_descriptor = open(inFilePath, O_RD | O_CREAT | O_TRUNC, 0640);
            // dup2()
            argCounter += 2;
            // if can't open either
            return false;
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
        }
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

        /*
        * 
        } else{
			printf("Child %d exited abnormally due to signal %d\n", childPid, WTERMSIG(childStatus));
		}
        
        */


        printf("PARENT(%d): child(%d) terminated. Exiting\n", getpid(), spawnPid);
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
        }
        execvp(current->binary, current->arguments);
        // exec only returns if there is an error
        perror("execvp");
        exit(1);
    default:
        // In the parent process
        // Wait for child's termination
        printf("PARENT(%d) launched BACKGROUND child(%d)\n", getpid(), spawnPid);
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
        printf("Looking at bg pid %d", iter->pid);
        int childStatus;
        int childReturn= waitpid(iter->pid, &childStatus, WNOHANG);
        if (childReturn) {
            if (WIFEXITED(childStatus))
            {
                int status = WEXITSTATUS(childStatus);
                printf(" !!!!!!!!!!!!!!! Returned pid %d with status %d\n", childReturn, status);

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
                free(hold);
                iter = iter->next;
            } 
            else
            {
               // we're at the start so we set the head to the following
                shell->bgActive = iter->next;
                struct bgProcess* hold = iter;
                free(hold);
                iter = iter->next;
            }
            // iter is now a new node and previous is directly before our new iter
            continue;
        }
        previous = iter;
        iter = iter->next;
    }
}

int main(int argc, char* argv[])
{
    // initialize our shellAttributes struct to store info the shell as a whole needs
    struct shellAttributes* shell = malloc(sizeof(struct shellAttributes));
    shell->bgActive = NULL;
    shell->lastForegroundStatus = 0;
    bool run = true;
    while (run)
    {
        // while backgroundProc->next != NULL - waitid nhohang for all.
        fflush(stdout);
        checkBackgroundProcs(shell);
        printf(": ");
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
        
        printf("binary: %s\n", currCommand->binary);
        printf("arguments: \n");
        int argCounter = 0;
        while (currCommand->arguments[argCounter] != '\0')
        {
            printf("index %d -> %s \n", argCounter, currCommand->arguments[argCounter]);
            argCounter++;
        }
        printf("\n");

        wireIORedirection(currCommand);

        printf("Exiting for testing io stuff");
        exit(1);
        
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
        argCounter = 0;
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
