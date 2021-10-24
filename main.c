#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>



struct shellAttributes
{
    char* running;
    int lastForegroundStatus; // set to 0 at the start
};

struct process
{
    char* name;
    int pid;
    int exitStatus;
    struct process* next;
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

    printf("--------------new: ");
    count = 0;
    while (expanded[count] != '\0') {
        printf("%c", expanded[count]);
        count++; //                                             ----------------------------------------------------- !
    }
    printf(" :new--------------");
    return expanded;
}

void parseCommand(char* input, struct command* currCommand)
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
            if (strcmp("#", token) == 0)
            {
                continue;
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
            printf("index 0 %s\n", currCommand->arguments[0]);
            argCounter++;
            continue;
        }
        

        currCommand->arguments[argCounter] = malloc(strlen(token) + 1);
        //currCommand->arguments[argCounter] = token;
        strcpy(currCommand->arguments[argCounter], token);
        printf("index %d : %s\n", argCounter, currCommand->arguments[argCounter]);
        argCounter++;
    }

    
    currCommand->background = false;
    if (currCommand->arguments != NULL)
    {
        char* lastVal = currCommand->arguments[argCounter - 1];
        printf("last val in args is %s\n", lastVal);
        if (strcmp(lastVal, "&") == 0)
        {
            printf("Yeah looking backgroundish to me\n");
            currCommand->background = true;
        }
    }
}

bool handleBuiltIns(struct shellAttributes* currShell, struct command* current)
{
    // changes directory based on argument of relative or absolute path
    // if no path is provided change to the HOME directory
    if (strcmp(current->binary, "cd") == 0)
    {
        
        printf("cd stuff\n");
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

void wireRedirection()
{
    // dup2 shit - You must do any input and/or output redirection using dup2(). The redirection must be done before using exec() to run the command.
    printf("OMFG");
}

void executeForeground(struct shellAttributes* shell, struct command* current)
{
    // execlp() or execvp() int execvp(const char* command, char* argv[]);
    // The second argument (argv) represents the list of arguments to command. This is an array of char* strings.

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
        printf("CHILD(%d) running ls command\n", getpid());
        // Replace the current program with "/bin/ls"
        execvp(current->binary, current->arguments);
        // exec only returns if there is an error
        perror("execvp");
        exit(2);
        break;
    default:
        // In the parent process
        // Wait for child's termination
        printf("spawned pid == %d\n", spawnPid);
        spawnPid = waitpid(spawnPid, &childStatus, 0);

        if (WIFEXITED(childStatus))
        {
            shell->lastForegroundStatus = WEXITSTATUS(childStatus);
        }
        printf("%d status code\n", shell->lastForegroundStatus);
        printf("PARENT(%d): child(%d) terminated. Exiting\n", getpid(), spawnPid);
        break;
    }
}

void executeBackground(struct command* current)
{
    printf("Launching background process\n");


}

int main(int argc, char* argv[])
{
    // initialize our shellAttributes struct to store info the shell as a whole needs
    struct shellAttributes* shell = malloc(sizeof(struct shellAttributes));
    shell->lastForegroundStatus = 0;
    bool run = true;
    while (run)
    {
        // while backgroundProc->next != NULL - waitid nhohang for all.

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
        parseCommand(line, currCommand);
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

        
        if (!handleBuiltIns(shell, currCommand)) // need to pass shell also if we need to close procs or return exit code
        {
            if (currCommand->background)
            {
                executeBackground(currCommand);
            }
            else
            {
                executeForeground(shell, currCommand); // pass shell to store status and current command
            }
        }
        


        /// YOU NEED TO FREE ALL THIS SHIT YOUVE BEEN ALLOCATING
    }
    return EXIT_SUCCESS;
}
