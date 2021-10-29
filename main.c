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

/*
* Global variables for handling SIGSTP correctly by the shell process
*/
bool allowBackground = true;
int lastForegroundPid = 0;

/*
* Attributes for the shell needed throughout the life of the program
*/
struct shellAttributes
{
    int* backgroundPids;
    int bgArraySize;
    // set to 0 at the start
    int lastForegroundStatus;
    bool lastExitFromSignal;
    int lastSignalStatus;
};

/*
* Attributes needed for every command run from the shell
*/
struct command 
{
    char* binary;
    char** arguments;
    int status;
    bool background;
};

/*
* handles SIGTSTP when recieved by the shell process
*/
void handle_SIGTSTP_shell(int signo)
{
    
    // check if the last foreground ran process is still running. block with wait if so
    if (lastForegroundPid)
    {
        int childStatus;
        int pid;
        pid = waitpid(lastForegroundPid, &childStatus, 0);
        printf("I waited up here to and got -> ");
        if (WIFEXITED(childStatus))
        {
            printf("Something different wtf %d", WEXITSTATUS(childStatus));
        }
        else
        {
            printf("diff??? terminated by signal %d\n", WTERMSIG(childStatus));
            fflush(stdout);
        }

    }

    // based on the allowBackground status flip to deny background if previously allowed or flip to allow from denied
    if (allowBackground)
    {
        char* message = "Entering foreground-only mode (& is now ignored)\n ";
        allowBackground = false;
        write(STDOUT_FILENO, message, 50);
        fflush(stdout);
    }
    else
    {
        char* message = "\nExiting foreground-only mode\n ";
        allowBackground = true;
        write(STDOUT_FILENO, message, 30);
        fflush(stdout);
    }
    // getline is normally interrupted and restarted based on SA RESTART
    // reprint out the prompt for the user
    write(STDOUT_FILENO, ": ", 2);
    fflush(stdout);
}

/*
* Accepts a string and replaces any instances of $$ anywhere in the string with the 
* pid of the process running
*/
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

/*
* Accepts user input from getline, tokenizes based on whitespace, and populates currCommmand
* with the binary (first arg in input), arguments as an array of input tokenized, and determines
* if commands are to be initiated in the background
*/
bool parseCommand(char* input, struct command* currCommand)
{
    char* saveptr = input;
    char* token;
    
    bool firstProcessed = false;
    int argCounter = 0;

    while ((token = strtok_r(saveptr, " \n", &saveptr)))
    {
        // perform character expansion for instances of $$ and set token to returned char array pointer
        token = charExpansion(token);
        // check for comments, pull the binary, initialize/start filling the arguments array
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
        
        // fill the arguments array with all the remaining tokens
        currCommand->arguments[argCounter] = malloc(strlen(token) + 1);
        strcpy(currCommand->arguments[argCounter], token);
        argCounter++;
    }
    currCommand->arguments[argCounter] = '\0';

    // check if the command indicates a background process
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

/*
* Checks if the binary is a built in command and returns if a built in was ran.
* Contains the commands cd, status, and exit
*/
bool handleBuiltIns(struct shellAttributes* currShell, struct command* current)
{
    // changes directory based on argument of relative or absolute path
    // if no path is provided changes to the HOME directory
    if (strcmp(current->binary, "cd") == 0)
    {
        // change to HOME directory if the only arg is cd
        if (current->arguments[1] == NULL) 
        {
            char* homeDir = getenv("HOME");       

            chdir(homeDir);
        } 
        // ignores any extra arguments provided
        else
        {
            chdir(current->arguments[1]);
        }
        return true;
    }
    // status command returns the exit value or signal value stored in the shell structure
    // based on the last foregound commands exit or signal termination status
    else if (strcmp(current->binary, "status") == 0)
    {
        if (currShell->lastExitFromSignal)
        {
            printf("terminated by signal %d\n", currShell->lastSignalStatus);
        }
        else
        {
            printf("exit value %d\n", currShell->lastForegroundStatus);
        }
        fflush(stdout);
        return true;
    }
    // exit kills all processes the shell has started and exits the shell program
    else if (strcmp(current->binary, "exit") == 0)
    {
        // kill all currently running child procs before exiting
        for (int i = 0; i < currShell->bgArraySize; i++)
        {
            if (!currShell->backgroundPids[i])
            {
                continue;
            }
            kill(currShell->backgroundPids[i], SIGKILL);
        }
        exit(0);
    }
    return false;
}

/*
* Parses the current arguments and finds < and > to pull arguments for redirection and
* remove from the arguments array. The array is rewritten to an array with IO arguments removed
* and dup2 is used to redirect to the files specified after the shell opens the files
*/
bool wireIORedirection(struct command* currCommand)
{
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
                printf("cannot open %s for input\n", inFilePath);
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
                printf("cannot open %s for output\n", outFilePath);
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
            // for non io redirection arguments copy to the new array normally in the next available position
            ioArguments[ioCounter] = malloc(strlen(currCommand->arguments[argCounter]) + 1);
            strcpy(ioArguments[ioCounter], currCommand->arguments[argCounter]);
            ioCounter++;
            argCounter++;
        }
    }
    ioArguments[ioCounter] = '\0';

    // wire stdin and out for background processes to /dev/null if not user redirected
    if (currCommand->background) 
    {
        char const devNull[] = "/dev/null";
        if (!wiredStdin)
        {
            int fd = open(devNull, O_RDONLY);
            if (fd == -1)
            {
                printf("cannot open %s for input\n", devNull);
                fflush(stdout);
                return false;
            }
            // Use dup2 to point in file to standard in - based off canvas example for io
            int result = dup2(fd, 0);
            if (result == -1)
            {
                perror("dup2");
                fflush(stderr);
                return false;
            }
        }
        if (!wiredStdout)
        {
            int fd = open(devNull, O_WRONLY);
            if (fd == -1)
            {
                printf("cannot open %s for output\n", devNull);
                fflush(stdout);
                return false;
            }
            // use dup2 to point stdout to target file - based off canvas example for io
            int result = dup2(fd, 1);
            if (result == -1) {
                perror("dup2()");
                fflush(stderr);
                return false;
            }
        }
    }
    
    // free our prior args and replace with ioArgs
    while (currCommand->arguments[argCounter] != '\0')
    {
        free(currCommand->arguments[argCounter]);
        argCounter++;
    }
    free(currCommand->arguments);
    // overwrite our existing arguments with the new io arguments
    currCommand->arguments = ioArguments;
    return true;
}

/*
* Execute a commmand in the foreground based on the binary and arguments in the passed
* commmand structure. Block the shell process by waiting on the spawned off process
* until it completes. Store the exit status or termination signal in the shell structuree.
*/
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
            // exit from the forked child when io redirection fails
            exit(1);
        }

        // Initialize SIGTSTP_action struct to be empty - code based off canvas signals exploration
        struct sigaction SIGTSTP_action = { 0 };
        // SET SIGTSTP to be ignored for children
        SIGTSTP_action.sa_handler = SIG_IGN;
        sigfillset(&SIGTSTP_action.sa_mask);
        SIGTSTP_action.sa_flags = 0;
        sigaction(SIGTSTP, &SIGTSTP_action, NULL);

        // set signal for SIGINT for foreground children to default behavior based off canvas signal exploration
        struct sigaction SIGINT_action = { 0 };
        // SET SIGINT to be ignored
        SIGINT_action.sa_handler = SIG_DFL;
        sigfillset(&SIGINT_action.sa_mask);
        SIGINT_action.sa_flags = 0;
        sigaction(SIGINT, &SIGINT_action, NULL);

        execvp(current->binary, current->arguments);
        perror("execvp");
        // exit status of 1 when binary not found
        exit(1);
    default:
        // In the parent process
        // Wait for child's termination
        lastForegroundPid = spawnPid;
        spawnPid = waitpid(spawnPid, &childStatus, 0);

        if (WIFEXITED(childStatus))
        {
            shell->lastForegroundStatus = WEXITSTATUS(childStatus);
            shell->lastExitFromSignal = false;
        }
        else 
        {
            shell->lastExitFromSignal = true;
            shell->lastSignalStatus = WTERMSIG(childStatus);
            printf("terminated by signal %d\n", shell->lastSignalStatus);
            fflush(stdout);
        }
        break;
    }
}

/*
* Execute a commmand in the background based on the binary and arguments in the passed
* commmand structure. Do not wait on the process spawned.
*/
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
            exit(1);
        }

        // SIGINT is already set to be ignored from the parent
        // set signal for SIGTSTP for background children to ignore based off canvas signal exploration
        struct sigaction SIGTSTP_action = { 0 };
        // SET SIGTSTP to be ignored
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
        printf("background pid is %d\n", spawnPid);
        fflush(stdout);
        return spawnPid;
    }

}

/*
* Iterate over the pids for all background processes started and
* call waitpid non blocking. If they've returned pull the exit status 
* or signal termination status. Replace in the background pid array with 
* 0 so they are not reprocessed.
*/
void checkBackgroundProcs(struct shellAttributes* shell)
{
    for (int i = 0; i < shell->bgArraySize; i++)
    {
        if (!shell->backgroundPids[i])
        {
            continue;
        }
        int childStatus;
        int childReturn = waitpid(shell->backgroundPids[i], &childStatus, WNOHANG);
        if (childReturn)
        {
            printf("background pid %d is done: ", shell->backgroundPids[i]);
            if (WIFEXITED(childStatus))
            {
                int bgExitStatus = WEXITSTATUS(childStatus);
                printf("exit value %d\n", bgExitStatus);
            }
            else
            {
                int bgSignalStatus = WTERMSIG(childStatus);
                printf("terminated by signal %d\n", bgSignalStatus);
            }
            fflush(stdout);
            shell->backgroundPids[i] = 0;
        }        
    }
}

/*
* Entry point for the shell. Initializes our shell and command structures.
* Establishes signal action handlers. Repeatedly runs through prompting the user
* and initiating built ins or starting new processes.
*/
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
    // prevent errors from getline or other interuptted calls
    SIGTSTP_action.sa_flags = SA_RESTART;
    // Install our signal handler
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // initialize our shellAttributes struct to store info the shell as a whole needs
    struct shellAttributes* shell = malloc(sizeof(struct shellAttributes));
    shell->backgroundPids = malloc(2 * sizeof(int));
    shell->bgArraySize = 0;
    shell->lastForegroundStatus = 0;
    shell->lastExitFromSignal = false;
    shell->lastSignalStatus = 0;
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
                // store pid of background process started
                shell->backgroundPids[shell->bgArraySize] = executeBackground(currCommand);
                shell->backgroundPids = realloc(shell->backgroundPids, (2 + shell->bgArraySize) * sizeof(int));
                shell->bgArraySize += 1;
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
