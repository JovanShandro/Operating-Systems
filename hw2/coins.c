/*
    Please compile the code using:
    gcc -pthread -o <name> coins.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

// Default values for people and number of flips
int P = 100, N = 10000;

// Initializing mutex variable
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t coin_mutex[20];

// Coin array
char coins[20] = "0000000000XXXXXXXXXX";

// Mutex lock and unlock wrappers
void lock(pthread_mutex_t* mutex);
void unlock(pthread_mutex_t* mutex); 

// Functions mentioned in the hw pdf
void run_threads(int n, void* (*proc)(void*));
double timeit(int n, void* (*proc)(void *));

// The 3 strategy functions
void *strategy1(void *args);
void *strategy2(void *args);
void *strategy3(void *args);

// Coin fliping function
char flip_coin(char coin);


int main(int argc, char* argv[])
{
    int opt;
    char *endptr; // needed for strtol
    double time;  // used to measure time for each strtegy
    
    // Parse command line options
    while((opt = getopt(argc, argv, "n:p:")) != -1)
    {
        switch (opt)
        {
        // Option n
        case 'n':
            N = strtol(optarg, &endptr, 10);
            if(*endptr != '\0')
            {
                fprintf(stderr, "Syntax error, invalid number of flips\n");
                _exit(EXIT_FAILURE);
            }else if(errno)
            {
                fprintf(stderr, "Invalid number of flips '%s'\n", strerror(errno));
                _exit(EXIT_FAILURE);
            }else if(N <= 0)
            {
                fprintf(stderr, "Invalid number, the entered flips nr must be positive\n");
                _exit(EXIT_FAILURE);
            }            
            break;
        // Option p  
        case 'p':
            P = strtol(optarg, &endptr, 10);
            if(*endptr != '\0')
            {
                fprintf(stderr, "Syntax error, invalid number of P\n");
                _exit(EXIT_FAILURE);
            }else if(errno)
            {
                fprintf(stderr, "Invalid number of P '%s'\n", strerror(errno));
                _exit(EXIT_FAILURE);
            }else if(P <= 0)
            {
                fprintf(stderr, "Invalid number, the entered poeple nr must be positive\n");
                _exit(EXIT_FAILURE);
            }
            break;
        //Others
        default:
            fprintf(stderr, "Error! No such option!!\n");
            _exit(EXIT_FAILURE);
            break;
        }
    }
    // Initialize the mutexes
    for(int i = 0; i<20; i++)
        pthread_mutex_init(&coin_mutex[i],NULL);

    /* 
        Run the 3 strategies and print the times taken.
        For displaying time in ms, I have used .3 precision
        as it seems as thats how it is in the example
        execution provided in the hw paper
    */

    printf("coins: %s (start - global lock)\n", coins);
    time = timeit(P, strategy1);
    printf("coins: %s (end - global lock)\n", coins);
    printf("%d threads  x %d flips: %.3lf ms\n\n", P, N, time);

    // coins array should be reseted by now
    printf("coins: %s (start - iteration lock)\n", coins);
    time = timeit(P, strategy2);
    printf("coins: %s (end - iteration lock)\n", coins);
    printf("%d threads  x %d flips: %.3lf ms\n\n", P, N, time);
    
    // coins array should be reseted by now
    printf("coins: %s (start - coin lock)\n", coins);
    time = timeit(P, strategy3);
    printf("coins: %s (end - coin lock)\n", coins);
    printf("%d threads  x %d flips: %.3lf ms\n\n", P, N, time);

    return 0;
}

// 3 Strategies
void *strategy1(void *args)
{
    // Using global lock
    lock(&mutex);
    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < 20; j++)
        {
            coins[j] = flip_coin(coins[j]);
        }
    }
    unlock(&mutex);
    pthread_exit(0);
}

void *strategy2(void *args)
{
    // Lock for each iteration
    for(int i = 0; i < N; i++)
    {
        lock(&mutex);
        for(int j = 0; j < 20; j++)
        {
            coins[j] = flip_coin(coins[j]);
        }
        unlock(&mutex);
    }
    pthread_exit(0);
}

void *strategy3(void *args)
{
    // A lock for every coin
    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < 20; j++)
        {
            lock(&coin_mutex[j]);
            coins[j] = flip_coin(coins[j]);
            unlock(&coin_mutex[j]);
        }
    }
    pthread_exit(0);
}

char flip_coin(char coin)
{
    return (coin =='X') ? ('0') : ('X');
}

// Similar function is on the slides
void run_threads(int n, void* (*f)(void*))
{
    int TID;
    pthread_t thread[n];
    // Create the n threads
    for (int i = 0; i < n; i++) 
    {
        TID = pthread_create(&thread[i], NULL, f, NULL);
        if (TID) 
        {
            fprintf(stderr, "Error while creating thread number %d: %s\n", i, strerror(TID));
            _exit(EXIT_FAILURE);
        }
    }
    // Join the threads
    for (int i = 0; i<n; i++) 
    {
        if (thread[i]) 
        {
            (void) pthread_join(thread[i], NULL);
        }
    }
}

double timeit(int n, void* (*proc)(void *))
{
    clock_t t1, t2;
    t1 = clock();
    run_threads(n, proc);
    t2 = clock();
    return ((double) t2 - (double) t1) / CLOCKS_PER_SEC * 1000;
}

void lock(pthread_mutex_t* mutex) 
{
    int r = pthread_mutex_lock(mutex);
    if(r)
    {
        fprintf(stderr, "Error on locking mutex!");
        exit(EXIT_FAILURE);
    }
}
void unlock(pthread_mutex_t* mutex) 
{
    int r = pthread_mutex_unlock(mutex);
    if(r)
    {
        fprintf(stderr, "Error on locking mutex!");
        exit(EXIT_FAILURE);
    }
}
