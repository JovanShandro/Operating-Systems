// run with gcc -pthread bar.c queue.c queue.h

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "queue.h"

/*
    Variables for keeping track of the number of detectives
    and clients that are inside the bar for the moment.
*/

int nr_det = 0, nr_cl = 0;

/*
    Variables to take care that the waits on conditional variables work.
*/

int turn1 = 0, turn2 = 0;

/*
    Since only one detective is enough to get all clients we need only
    one det condition variable. On the other side when a client
    enters and finds only one detective he/she has to signal only one
    detective so we create an array of condition variables, one for
    each detective
*/

pthread_mutex_t mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  det    = PTHREAD_COND_INITIALIZER;
pthread_cond_t *cls;

/*
    2 Queues to keep track of the detectives and clients inside the bar
*/
Queue *client_queue, *detective_queue;

static const char *progname = "detectives_and_clients";

// Enum to help us distingush between clients and detectives
enum Type {client, detective};

/*
    Person struct to implement clients and detectives
*/
struct Person {
    enum Type type;
    char name[16];
    int count;
};

/*
    Lock a mutex with error handling
*/
static void lock(pthread_mutex_t *mutex)
{
    int r = pthread_mutex_lock(mutex);
    if(r)
    {
        fprintf(stderr, "%s: Error on mutex lock\n", progname);
        exit(EXIT_FAILURE);
    }
}

/*
    Unlock a mutex with error handling
*/
static void unlock(pthread_mutex_t *mutex)
{
    int r = pthread_mutex_unlock(mutex);
    if(r)
    {
        fprintf(stderr, "%s: Error on mutex unlock\n", progname);
        exit(EXIT_FAILURE);
    }
}

/*
    Wait on a condition variable with error handling
*/
static void wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    int r = pthread_cond_wait(cond, mutex);
    if(r)
    {
        fprintf(stderr, "%s: Error on wait function\n", progname);
        exit(EXIT_FAILURE);
    }
}

/*
    Signal one condition variable with error handling
*/
static void signal_one(pthread_cond_t *cond)
{
    int r = pthread_cond_signal(cond);
    if(r)
    {
        fprintf(stderr, "%s: %s(): Error on conditional variable waiting\n", progname, __func__);
        exit(EXIT_FAILURE);
    }
}

/*
    Signal all threads waiting on one condition variable with error handling
*/
static void signal_all(pthread_cond_t *cond)
{
    int r = pthread_cond_broadcast(cond);
    if(r)
    {
        fprintf(stderr, "%s: %s() Error while broadcasting\n", progname, __func__);
        exit(EXIT_FAILURE);
    }
}

/*
    Client thread function (called by enjoy_life)
*/
static void client_visit_bar(struct Person* person)
{
    lock(&mutex);
    // Update clients queue
    enqueue(person->name, client_queue);

    printf("bar: %5d c %5d d %5s entering\n", nr_cl, nr_det, person->name);
    nr_cl++;

    if(nr_det > 0)
    {
        char first_det[6];
        // Get the detective that came first
        dequeue(&first_det, detective_queue);
        
        int id;
        // Get Id of the detective who came first
        if(1 != sscanf(first_det, "%*[^0123456789]%d", &id))
        {
            fprintf(stderr, "%s: Error on sscanf\n", progname);
            exit(EXIT_FAILURE);
        }
        // Signal detective who came first
        signal_one(&cls[id]);
        turn1 = 1;

        printf("bar: %5d c %5d d %5s picked up by %s\n", nr_cl, nr_det, person->name, first_det);
        // Remove this client, we use first_det just for the sake of not creating a new variable
        dequeue_back(&first_det, client_queue);
    }else
    {
        printf("bar: %5d c %5d d %5s waiting...\n", nr_cl, nr_det, person->name);
        turn2 = 0;
        // Wait until a detective enters
        while (turn2 == 0)
            wait(&det, &mutex);
        
        printf("bar: %5d c %5d d    ...%s waking up\n", nr_cl, nr_det, person->name);        
    }
    nr_cl--;

    printf("bar: %5d c %5d d %5s leaving\n", nr_cl, nr_det, person->name);
    unlock(&mutex);
}

/*
    Detective thread function (called by enjoy_life)
*/
static void detective_visit_bar(struct Person* person)
{
    lock(&mutex);
    // Update detective queue
    enqueue(person->name, detective_queue);

    printf("bar: %5d c %5d d %5s entering\n", nr_cl, nr_det, person->name);
    nr_det++;

    if(nr_cl > 0 && !queue_is_empty(client_queue))
    {
        // Signal all clients waiting for a detective
        signal_all(&det);
        turn2 = 1;

        printf("bar: %5d c %5d d %5s picking clients", nr_cl, nr_det, person->name);
        // empty the clients queue
        char dummy[6];
	    while (!queue_is_empty(client_queue)) {
		    dequeue(&dummy, client_queue);
            // print all picked up clients
            printf(" %s", dummy);
	    }
        printf("\n");
        // Update queue as detective will leave
        dequeue(&dummy, detective_queue);
    }else
    {
        printf("bar: %5d c %5d d %5s waiting...\n", nr_cl, nr_det, person->name);
        
        //find detective nr
        int id;
        if(1 != sscanf(person->name, "%*[^0123456789]%d", &id))
        {
            fprintf(stderr, "%s: Error on sscanf\n", progname);
            exit(EXIT_FAILURE);
        }
        // Wait for a client
        turn1 = 0;
        while (turn1 == 0)
            wait(&cls[id], &mutex);
        printf("bar: %5d c %5d d    ...%s waking up\n", nr_cl, nr_det, person->name);        
    }
     
    nr_det--;
    printf("bar: %5d c %5d d %5s leaving\n", nr_cl, nr_det, person->name);
    unlock(&mutex);
}

/*
    enjoy_life function based on the pseudocode in the pdf
*/
static void *enjoy_life(void *args)
{
    int r;
    struct Person* person = (struct Person*) args;
    
    // Seed rand nr generator
    srand(time(0));

    while(1)
    {
        switch (person->type)
        {
        case client:
            client_visit_bar(person);
            break;
        case detective:
            detective_visit_bar(person);
            break;    
        default:
            break;
        }
        if( (r = usleep(rand()%100000)) == -1)
        {
            fprintf(stderr, "%s: %s(): Error on usleep function %s\n",
                progname, __func__, strerror(r));
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

/*
    Function to create all the threads
*/
static int run (int nc, int nd)
{
    int r, n = nc + nd;
    pthread_t thread[n];
    struct Person people[n];
    char int_string[16];    
    // Create nc clients
    for (int i = 0; i<nc; i++)
    {
        people[i].type = client;
        people[i].count = i;
        sprintf(int_string, "c%d", i);
        strncpy(people[i].name, int_string, 16);
    }
    // Create nd detectives
    for (int i = 0; i<nd; i++)
    {
        people[nc + i].type = detective;
        people[nc + i].count = i;
        sprintf(int_string, "d%d", i);
        strncpy(people[nc + i].name, int_string, 16);
    }
    // Create n threads
    for(int i = 0; i<n; i++)
    {
        r = pthread_create(&thread[i], NULL, enjoy_life, &people[i]);

        if(r)
        {
            fprintf(stderr, "%s: %s(): unable to create thread %d: %s\n",
                progname, __func__, i, strerror(r));
            return EXIT_FAILURE; 
        }
    }
    // Join threads
    for (int i = 0; i<n; i++)
    {
        if(thread[i])
        {
            r = pthread_join(thread[i], NULL);
            if(r)
            {
                fprintf(stderr, "%s: %s(): unable to join thread %d: %s\n",
                    progname, __func__, i, strerror(r));
            }
        }
    }
    return EXIT_SUCCESS;
}

/*
    MAIN
*/
int main(int argc, char* argv[])
{
    int opt, nc = 1, nd = 1, r;
    char *end;
    while( (opt = getopt(argc, argv, "c:d:h") ) != -1)
    {
        switch (opt)
        {
        case 'c':
            nc = (int) strtol(optarg, &end, 10);
            if(*end || nc < 1)
            {
                fprintf(stderr, "%s: Number of clients must be "
                            "a positive decimal number!\n", progname);
                exit(EXIT_FAILURE);
            }
            break;
        case 'd':
            nd = (int) strtol(optarg, &end, 10);
            if(*end || nd < 1)
            {
                fprintf(stderr, "%s: Number of detectives must be "
                            "a positive decimal number!\n", progname);
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            printf("Usage: %s [-c clients] [-d detectives] [-h help]\n", progname);
            exit(EXIT_FAILURE);
            break;
        }
    }   
    // Initialize the array of condition variables 
    cls = (pthread_cond_t* )malloc(nd * sizeof(pthread_cond_t));
    for(int i = 0; i < nr_det; i++)
    {
        r = pthread_cond_init(&cls[i], NULL);
        if(r)
        {
            fprintf(stderr, "%s: Error while initializing conditional "
                "variable number %d!\n", progname, i);
            exit(EXIT_FAILURE);
        }
    }
    // initialize queues
    client_queue = (Queue* )malloc(sizeof(Queue));
    detective_queue = (Queue* )malloc(sizeof(Queue));
    initialize_queue(client_queue);
    initialize_queue(detective_queue);
    
    return run(nc, nd);
}
