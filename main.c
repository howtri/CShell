#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>


struct shellStructures
{
    char* running;
    int lastStatus; // set to 0 at the start
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

        char* saveptr = line;
        char* token;

        if (!lineSize)
        {
            printf("Empty yo");
            continue;
        }

        // create our struct for the command we'll process
        struct command* currCommand = malloc(sizeof(struct command));
        bool firstProcessed = false;
        bool argsInit = false;

        while ((token = strtok_r(saveptr, " ", &saveptr)))
        {
            printf("%s the token\n", token);
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
                strcat("\0");
                continue;
            }

            currCommand->arguments = realloc(currCommand->arguments, strlen(currCommand->arguments) + strlen(token) + 1);
            strcat(currCommand->arguments, "%s\0", token);

            // someway to check if last one is & for background?
        }

        printf("%s for our command struct", currCommand->binary);
        // strtok if first arg = # continue
        free(line);
    }
    return EXIT_SUCCESS;
}
