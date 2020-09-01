#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

int main(int argc, char* argv[])
{
    // bell is 1 if the option -b is found, same goes for e_mode and -e
    int opt, bell = 0, e_mode = 0; 
    //status of child processes
    int status; 
    pid_t PID, endPID;
    // seconds for each sleep
    float seconds = 2.0; 
    //the arguments for the called program
    char *ex[argc]; 

    // handle command line option
    while( (opt = getopt(argc, argv, "+n:be")) != -1)
    {
        switch (opt)
        {
            case 'n':
                if((seconds = atof(optarg)) <= 0)
                {
                    fprintf(stderr, "Error on value entered by -n\n");
                    exit (EXIT_FAILURE);
                }
                break;
            case 'b':
                bell = 1;
                break;
            case 'e':
                e_mode = 1;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
    if(optind >= argc)
        exit(EXIT_FAILURE);        
    // initialize the arguments for the program that will be watched
    int i =0;
    for(; optind < argc; optind++)
    {
        ex[i++] = argv[optind];
    }
    ex[i++] = NULL;

    // main loop    
    while(1)
    {
        PID = fork();
        
        if(PID == -1) //error ocurred
            exit(EXIT_FAILURE);
        if(PID == 0) //child process
        { 
            execvp(ex[0], ex);
            // in failure case
            if(bell)
            {
                if(putc('\a', stdout) == EOF)
                {
                    fprintf(stderr, "Putc error!\n");
                    exit(EXIT_FAILURE);
                }
                if(ferror(stdout))
                {
                    fprintf(stderr, "Error state in !\n");
                    exit(EXIT_FAILURE);
                }
            }
                
            if(e_mode)
                exit(EXIT_FAILURE); 
        }
        if(PID > 0) //parent process
        {
            endPID = waitpid(PID, &status, WUNTRACED);
            // in case the child exits with error
            if(WIFEXITED(status) && (WEXITSTATUS(status) != 0)  && bell)
            {
                if(putc('\a', stdout) == EOF)
                {
                    fprintf(stderr, "Putc error!\n");
                    exit(EXIT_FAILURE);
                }
                if(ferror(stdout))
                {
                    fprintf(stderr, "Error state in !\n");
                    exit(EXIT_FAILURE);
                }
            }
            // in case the child exits with error in error mode
            if(WIFEXITED(status) && (WEXITSTATUS(status) != 0)  && e_mode)
            {
                fprintf(stderr, "Execution error on %s call!\n", ex[0]);
                exit(EXIT_FAILURE);
            }
            // waitpid error
            if(endPID == -1)
            {
                fprintf(stderr, "Waitpid error!\n");
                exit(EXIT_FAILURE);
            }
        }
        sleep(seconds);
    }
    return 0;
}
