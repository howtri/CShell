#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <unistd.h>



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

void parseCommand(char* input, struct command* currCommand)
{
    char* saveptr = input;
    char* token;
    
    bool firstProcessed = false;
    int argCounter = 0;

    while ((token = strtok_r(saveptr, " \n", &saveptr)))
    {
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
        char* workingDir[100];
        printf("cd stuff\n");
        // change to HOME directory if the only arg is cd
        if (current->arguments[1] == NULL) 
        {
            char* homeDir = getenv("HOME");
            printf("Home directory is %s\n", homeDir);
            printf("%s\n", getcwd(workingDir, 100));
            chdir(homeDir);
            printf("%s\n", getcwd(workingDir, 100));
        } 
        else // DO WE WANT TO CHECK THAT ITS EXACTLY 1?
        {
            printf("%s\n", getcwd(workingDir, 100));
            chdir(current->arguments[1]);
            printf("%s\n", getcwd(workingDir, 100));
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
    // dup2 shit
    printf("OMFG");
}

void executeForeground()
{
    // execlp() or execvp() int execvp(const char* command, char* argv[]);
    // The second argument (argv) represents the list of arguments to command. This is an array of char* strings.


}

void executeBackground()
{

}

int main(int argc, char* argv[])
{
    // initialize our shellAttributes struct to store info the shell as a whole needs
    struct shellAttributes* shell = malloc(sizeof(struct shellAttributes));
    shell->lastForegroundStatus = 0;
    bool run = true;
    while (run)
    {
        

        printf(": ");
        fflush(stdout);
        char* line = NULL;
        size_t len = 0;
        ssize_t lineSize = 0;
        lineSize = getline(&line, &len, stdin);

        if (!lineSize)
        {
            printf("Empty yo");
            continue;
        }

        // create our struct for the command we'll process
        struct command* currCommand = malloc(sizeof(struct command));
        parseCommand(line, currCommand);
        free(line);
        
        printf("binary: %s\n", currCommand->binary);
        printf("arguments: \n");
        int argCounter = 0;
        while (currCommand->arguments[argCounter] != NULL)
        {
            printf("index %d -> %s \n", argCounter, currCommand->arguments[argCounter]);
            argCounter++;
        }
        printf("\n");

        
        if (!handleBuiltIns(shell, currCommand)) // need to pass shell also if we need to close procs or return exit code
        {
            if (currCommand->background)
            {
                printf("Launching background process\n");
            }
            else
            {
                printf("Launching foreground process\n");
            }
        }
        


        /// YOU NEED TO FREE ALL THIS SHIT YOUVE BEEN ALLOCATING
    }
    return EXIT_SUCCESS;
}
