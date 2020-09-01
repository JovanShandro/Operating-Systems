/*
   To start run ./server <port nr>
   and then to test with a client 
   run nc localhos <port nr>
   The server can handle multiple 
   clients at once
*/

// To remove error on struct addrinfo
#define _DEFAULT_SOURCE

/*
    All methods starting with tcp_ are 'borrowed'
    from Prof. Schoenwaelder's source codes.
    (tcp.c and tcp.h)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>

#include <event2/event.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>

const char *progname = "server";
// needed for tokenizing by strtok
char separators[] = "._\v-\n! '\r;\f,\t"; 

// enum for type or response
enum resp_type { undefined, challenge, bye_message, right_guess, wrong_guess};

/*
    Array of all possible messages to send
*/
const char* msgs[] = 
{
   "Try again! Command not defined!!!\n", "C: %s",
   "M: You mastered %d/%d challenges. Good bye!\n",
   "Congratulation - challenge passed!\n",
   "Wrong guess - expected: %s\n"
};

typedef struct game_t 
{
   // Text got from fortune call
   char f_txt[32 + 1]; // also including \0
   char missing_word[32 + 1]; // worst case whole msg is a word
   // number of guesses
   int ng;
   // number of correct guesses
   int nc;
} Game;

// Each client holds a connection
typedef struct client 
{
   // fd of the socket
   int filedesc;
   // each connection has an event for the event loop
   struct event *event;
   // the game for each connection
   Game *game;
   // pointer to next connection in linked list
   struct client *next;
} Client;

/*
    Initialize event base and front of client list
*/
struct event_base *base;
Client *list_front = NULL;

/*
    Function to add or remove a client client(s)
*/
static Client* modify_list(Client **front, int filedesc, int flag);

/*
   Function to initialize pipe and run fortune command line
*/
static void fortune_init (Client* client);

/*
   Function to send response specified by the second param
   to the client in first param
*/
static void write_line   (Client* client, int resp);

/*
   Function to receive a line from the socket specified
   by first file descriptor
*/
static void recv_line    (evutil_socket_t event_file_desc, short evwhat, void *evarg);

/*
   Function to read the result of the fortune call and modify
   a random word to underlines
*/
static void modify_line  (evutil_socket_t event_file_desc, short evwhat, void *evarg);

/*
   Function to initialize connection with the provided 
   descriptor (socket)
*/
static void initialize   (evutil_socket_t event_file_desc, short evwhat, void *evarg);

// Functions borrowed from tcp.c
static ssize_t tcp_read   (int fd, void *buffer, size_t count);
static ssize_t tcp_write  (int fd, const void *buffer, size_t count);
static int     tcp_listen (const char *host, const char *port);
static int     tcp_accept (int fd);
static int     tcp_close  (int fd);

/*
    For the main function again special thanks to the professor
    for making this implementation possible. The event loop is 
    similar to the one the prof has done in the source code of daytimed.
*/
int main(int argc, char* argv[])
{
   int tfd;
   struct event *tev;
   const char* interfaces[] = {"localhost", NULL, "0.0.0.0", "::"};

   if(argc != 2)
   {
      fprintf(stderr, "%s: You have not entered enough arguments\n", progname);
      exit(EXIT_FAILURE);
   }
   // seed random nr generator
   srand(time(0));

   (void) daemon(0, 0);
   openlog(progname, LOG_PID, LOG_DAEMON);

   base = event_base_new();
   if(!base)
   {
      syslog(LOG_ERR, "creating event base failed");
      exit(EXIT_FAILURE);
   }

   for(int i = 0; interfaces[i]; i++)
   {
      tfd = tcp_listen(interfaces[i], argv[1]);
      
      if(tfd < 0){
         fprintf(stderr, "error %s %s: could listen to client\n", strerror(errno), progname);
         continue;
      }
      
      tev = event_new(base, tfd, EV_READ | EV_PERSIST, initialize, NULL);
      event_add(tev, NULL);
   }

   if(event_base_loop(base, 0) == -1)
   {
      syslog(LOG_ERR, "event loop failed");
      event_base_free(base);
      exit(EXIT_FAILURE);
   }

   closelog();

   event_base_free(base);

   return EXIT_SUCCESS;
}

/*
 * The tcp_read() function behaves like read(), but restarts
 * after signal interrupts and on other short reads.
 */

static ssize_t tcp_read(int fd, void *buffer, size_t count)
{
    size_t nread = 0;

    while (count > 0) 
    {
        int r = read(fd, buffer, count);
        if (r < 0 && errno == EINTR)
            continue;
        if (r < 0)
            return r;
        if (r == 0)
            return nread;
        buffer = (unsigned char *) buffer + r;
        count -= r;
        nread += r;
    }

    return nread;
}

/*
 * The tcp_write() function behaves like write(), but restarts
 * after signal interrupts and on other short writes.
 */

static ssize_t tcp_write(int fd, const void *buffer, size_t count)
{
    size_t nwritten = 0;

    while (count > 0) {
	int r = write(fd, buffer, count);
	if (r < 0 && errno == EINTR)
	    continue;
	if (r < 0)
	    return r;
	if (r == 0)
	    return nwritten;
	buffer = (unsigned char *) buffer + r;
	count -= r;
	nwritten += r;
    }

    return nwritten;
}

/*
 * Create a listening TCP endpoint. First get the list of potential
 * network layer addresses and transport layer port numbers. Iterate
 * through the returned address list until an attempt to create a
 * listening TCP endpoint is successful (or no other alternative
 * exists).
 */

static int tcp_listen(const char *host, const char *port)
{
    struct addrinfo hints, *ai_list, *ai;
    int n, fd = 0, on = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    n = getaddrinfo(host, port, &hints, &ai_list);
    if (n) {
        syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(n));
        return -1;
    }

    for (ai = ai_list; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }

#ifdef IPV6_V6ONLY
        /*
         * Some IPv6 stacks by default accept IPv4-mapped addresses on
         * IPv6 sockets and hence binding a port separately for both
         * IPv4 and IPv6 sockets fails on these systems by default.
         * This can be avoided by making the IPv6 socket explicitly an
         * IPv6 only socket.
         */
        if (ai->ai_family == AF_INET6) {
            (void) setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        }
#endif

        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;
        }
        close(fd);
    }

    freeaddrinfo(ai_list);

    if (ai == NULL) {
        syslog(LOG_ERR, "bind failed for port %s", port);
        return -1;
    }

    if (listen(fd, 42) < 0) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
	     close(fd);
        return -1;
    }

    return fd;
}

/*
 * Accept a new TCP client and write a msg about who was
 * accepted to the system log.
 */

static int tcp_accept(int listen)
{
    struct sockaddr_storage ss;
    socklen_t ss_len = sizeof(ss);
    char host[512];
    char serv[32];
    int n, fd;

    fd = accept(listen, (struct sockaddr *) &ss, &ss_len);
    if (fd == -1) {
        syslog(LOG_ERR, "accept failed: %s", strerror(errno));
        return -1;
    }

    n = getnameinfo((struct sockaddr *) &ss, ss_len,
                    host, sizeof(host), serv, sizeof(serv),
                    NI_NUMERICHOST);
    if (n) {
        syslog(LOG_ERR, "getnameinfo failed: %s", gai_strerror(n));
    } else {
        syslog(LOG_DEBUG, "client from %s%s%s:%s",
	       strchr(host, ':') == NULL ? "" : "[",
	       host,
	       strchr(host, ':') == NULL ? "" : "]",
	       serv);
    }

    return fd;
}

/*
 * Close a TCP client. This function trivially calls close() on
 * POSIX systems, but might be more complicated on other systems.
 */

static int tcp_close(int fd){
   return close(fd);
}

/*
    If flag is 0, a new client is added to the linked list
    with the given front(or front) and the given file desc, 
    else if flag is 1, the nodes with the given file desc
    are removed.
*/
static Client* modify_list(Client **front, int filedesc, int flag)
{
   if(!flag) // Add client
   {
      Client *current = NULL;

      // Case 1: Client list is empty
      if(*front == NULL)
      {
         *front = (Client*) malloc(sizeof(Client));
         if(!(*front))
         {
            fprintf(stderr, "Error on new client creation!!");
            exit(EXIT_FAILURE);
         }
         current = *front;
      }
      else // list is non-empty
      {
         current = *front;
         // Move until the end
         while(current->next != NULL)
            current = current->next;
         // Allocate space for new client
         current->next = (Client*) malloc(sizeof(Client));
         // Add the client
         current = current->next;
         current->next = NULL;
      }
      // Set event to null
      current->event = NULL;
      // Set current file desriptor
      current->filedesc = filedesc;
      // Allocate memory for the game of game_t 
      current->game = (Game*) malloc(sizeof(Game));
    
      // Both set to empty strings
      current->game->f_txt[0] = '\0';
      current->game->missing_word[0] = '\0'; 
      // Initialize the game of the new client
      current->game->ng = 0;
      current->game->nc = 0;
        
      // return the new client
      return current;
    } 
     else  // Remove client
    {
        Client *current = NULL;
        // If list is empty throw error
        if(*front == NULL)
        {
            fprintf(stderr, "The client list is currenty empty. "
                    "Client removal is not possible\n");
            exit(EXIT_FAILURE);
        }
    
        current = *front;
        // If front has to be removed
        if(current->filedesc == filedesc)
        {
            // Move front of list
            *front = (*front)->next;
            // Free the event of the client
            event_free(current->event);
            // Free the memory allocated
            free(current);
            return EXIT_SUCCESS;
        }
        // Else search the list to find the client
        while(current->next != NULL)
        {
            if(current->next->filedesc == filedesc)
            {
                // Remove the client
                Client* temp = current->next;
                current->next = current->next->next;
                // Free the event of the client
                event_free(temp->event);
                // Free the memory allocated for the client
                free(temp);
                break;
            }
            current = current->next;
        }
        return NULL; // empty return statement, not needed for the removal case
    }
}

static void modify_line(evutil_socket_t filedesc, short evwhat, void *evarg)
{
   /*
      buffer will store the values read from the event file desc
      temp is a temporary array used when tokenizing
      txt and t are needed for tokenizing with strtok
   */

   char buffer[128], temp[64], *txt, *t = NULL;
   int size, n, random_n;

   // Get current client client
   Client *current = (Client*) evarg;

   if((size = read(filedesc, buffer, 32)) < 0)
   {
      fprintf(stderr, "%s: Error failure on read sys call! (%s)\n", progname, strerror(errno));
      exit(EXIT_FAILURE);
   } 
   else 
   {
      buffer[size] = '\0';
      strcpy(current->game->f_txt, buffer);
   }
   // Get game of current client
   Game* st = current->game;
   // Find where separators are in the fortune string
   txt = st->f_txt;
   while(txt = strpbrk(txt, separators))
      txt++, n++;

   while(!t)
   {
      // get random number from 0 to n-1 then reinitialize n
      random_n = rand()%n;
      n = 0;
      // copy fortune text to the temporary buffer
      strcpy(temp, st->f_txt);
      // start tokenizing
      t = strtok(temp, separators);
      // full tokenizing 
      while(t)
      {
         if(n == random_n)
         {
            txt = strstr(st->f_txt, t);
            if(!txt) break;
            // Put underlines in the missing word
            memset(txt, '_', strlen(t));
            // Set missing word
            strcpy(st->missing_word, t);
            break;
         }
         n++;
         t = strtok(NULL, separators);
      }
   }
   write_line(current, 1);
}

static void fortune_init(Client* client)
{
   // id used for forking
   int id;
   // file desc for pipe
   int fd[2];
   // event struct to later create new event
   struct event *event;

   // create pipe
   pipe(fd);
   // create new event
   event = event_new(base, fd[0], EV_READ, modify_line, client);
   // add the event
   event_add(event, NULL);

   id = fork();
   if(id) //parent 
   { 
      // close the write endpoint of pipe 
      close(fd[1]);
   }
   else if(id == 0) //child
   {
      // close reading point as not needed
      close(fd[0]);
      // dup the writing endpoint to stdout
      dup2(fd[1], 1);
      // close writing endpoint
      close(fd[1]);
      // exec fortune call
      char* args[] = { "fortune", "-n", "32", "-s", NULL };
      execvp(args[0], args);
      // if execvp fails
      fprintf(stderr, "Error on fortune exec call");
      exit(EXIT_FAILURE);
   } 
   else // failure 
   {
      fprintf(stderr, "Fork failed, text is empty!!");
      client->game->f_txt[0] = '\0';
   }
}

static void write_line(Client* client, int type)
{
   char response[256];

   if      (type == 0)  strcpy (response, msgs[undefined]);
   else if (type == 1)  sprintf(response, msgs[challenge], client->game->f_txt);
   else if (type == 2)  sprintf(response, msgs[bye_message], client->game->nc, client->game->ng);
   else if (type == 3)  strcpy (response, msgs[right_guess]);
   else if (type == 4)  sprintf(response, msgs[wrong_guess], client->game->missing_word);

   if(tcp_write(client->filedesc, response, strlen(response)) < 0)
   {
      fprintf(stderr, "%s: write failed %s\n", progname, strerror(errno));
      exit(EXIT_FAILURE);
   }
}

static void recv_line(evutil_socket_t filedesc, short evwhat, void *evarg)
{
   // Get current client client
   Client *current = (Client*) evarg;
   /*
      buffer will hold the typed line
      guess will hold the guess only. Used for tokenizing
   */
   char line[256], *guess;

   // Read from file descripter
   int size;
   size = read(filedesc, line, sizeof(line));
   if(!size && size != 0)
   {
      fprintf(stderr, "%s: Failure on read sys call! (%s)\n", progname, strerror(errno));
      exit(EXIT_FAILURE);
   }

   // Parse the input string
   if(strstr(line, "R:") == line) // Guess
   {
      // Increment guess number
      current->game->ng++;
      // Get rid of the R: in the beginning
      memmove(line, line+2, strlen(line)-2);
      // Start tokenizing
      guess = strtok(line, separators);
      // If guess is wrong
      if(guess == NULL || strcmp(guess, current->game->missing_word) != 0)
         write_line(current, 4);
      else // guess is right
      {
         // increment number of correct guesses
         current->game->nc++;
         // send proper msg to client
         write_line(current, 3);
      }
      // Get fortune for next time
      fortune_init(current);
   } 
   else if(strstr(line, "Q:") == line) // Quit
   {
      // Send proper msg to client
      write_line(current, 2);
      // Free the event
      event_free(current->event);
      // Close the client
      tcp_close(filedesc);
      // Remove the client client
      modify_list(&list_front, filedesc, 1);
   } 
   else // unknown command
      write_line(current, 0);
   
}

static void initialize(evutil_socket_t event_file_desc, short evwhat, void *evarg)
{
   // Create a client client
   Client *newel;
   // Start messag
   char msg[] = "M: Guess the missing_word ____!\nM: Send your guess in the for 'R: word\\r\\n'.\n";

   // Connect to client
   int filedesc = tcp_accept((int)event_file_desc);
   if(!filedesc && (filedesc != 0))
   {
      fprintf(stderr, "%s: Error client was not possible: %s\n", progname, strerror(errno));
      exit(EXIT_FAILURE);
   }

   // Write the initial msg
   if((tcp_write(filedesc, msg, strlen(msg))) < 0) // if error
   {
      // print msg in stderr
      fprintf(stderr, "%s: Error! Failure on write sys call\n", progname);
      // close socket client
      tcp_close(filedesc);
      // exit
      exit(EXIT_FAILURE);
   }
   // Initialize client linked list by adding first client
   newel = modify_list(&list_front, filedesc, 0);
   // Create event on reading from client
   newel->event = event_new(base, filedesc, EV_READ | EV_PERSIST, recv_line, newel);
   // Add the event
   event_add(newel->event, NULL);
   // Generate fortune
   fortune_init(newel);
}
