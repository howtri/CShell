#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>


struct shellStructures
{
    char* running;
    int lastStatus; // set to 0 at the start
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
    char* arguments;
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
    bool argsInit = false;

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
            continue;
        }


        // initialize our first arg, after this we reallocate for all additional tokens
        if (!argsInit)
        {
            argsInit = true;
            currCommand->arguments = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currCommand->arguments, token);
            //strcat(currCommand->arguments, " ");
            continue;
        }

        currCommand->arguments = realloc(currCommand->arguments, strlen(currCommand->arguments) + strlen(token) + 1);
        strcat(currCommand->arguments, token);
        //strcat(currCommand->arguments, " ");
    }

    currCommand->background = false;
    if (currCommand->arguments != NULL)
    {
        char lastChar = currCommand->arguments[strlen(currCommand->arguments) - 3];
        printf("last val in args is %c\n", lastChar);
        if (lastChar == '&')
        {
            printf("Yeah looking backgroundish to me\n");
            currCommand->background = true;
        }
    }
}

void handleBuiltIns(struct command* current)
{
    if (strcmp(current->binary, "cd") == 0)
    {
        printf("cd\n");
    }
    else if (strcmp(current->binary, "status") == 0)
    {
        printf("status\n");
    }
    else if (strcmp(current->binary, "exit") == 0)
    {
        printf("exiting\n");
        exit(0);
    }
}
/*
*   entry point and launch menu function
*/
int main(int argc, char* argv[])
{
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
        printf("arguments: %s\n", currCommand->arguments);
        
        handleBuiltIns(currCommand); // need to pass shell also if we need to close procs or return exit code

    }
    return EXIT_SUCCESS;
}
