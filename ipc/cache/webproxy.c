#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <printf.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <stdlib.h>


//headers would go here
#include "cache-student.h"
#include "gfserver.h"

// note that the -n and -z parameters are NOT used for Part 1 */
// they are only used for Part 2 */
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 7)\n"                      \
"  -p [listen_port]    Listen port (Default: 25496)\n"                                 \
"  -s [server]         The server to connect to (Default: GitHub test data)\n"     \
"  -t [thread_count]   Num worker threads (Default: 34, Range: 1-420)\n"              \
"  -z [segment_size]   The segment size (in bytes, Default: 5701).\n"                  \
"  -h                  Show this help message\n"


// Options
static struct option gLongOptions[] = {
  {"server",        required_argument,      NULL,           's'},
  {"segment-count", required_argument,      NULL,           'n'},
  {"listen-port",   required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"segment-size",  required_argument,      NULL,           'z'},
  {"help",          no_argument,            NULL,           'h'},

  {"hidden",        no_argument,            NULL,           'i'}, // server side
  {NULL,            0,                      NULL,            0}
};


//gfs
static gfserver_t gfs;
//handles caches
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);
steque_t *shm_stq;

static void _sig_handler(int signo){
  if (signo == SIGTERM || signo == SIGINT){
    //cleanup could go here
    gfserver_stop(&gfs);

    while (steque_isempty(shm_stq) != 1) {
      free(steque_pop(shm_stq));
    }
    steque_destroy(shm_stq);
    exit(signo);
  }
}

int main(int argc, char **argv) {
  int option_char = 0;
  char *server = "https://raw.githubusercontent.com/gt-cs6200/image_data";
  unsigned int nsegments = 13;
  unsigned short port = 6200;
  unsigned short nworkerthreads = 33;
  size_t segsize = 5313;

  //disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGINT, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  // Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "s:qht:xn:p:lz:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(__LINE__);
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'i':
      //do not modify
      case 'O':
      case 'A':
      case 'N':
            //do not modify
      case 'k':
        break;
    }
  }


  if (server == NULL) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(__LINE__);
  }

  if (segsize < 824) {
    fprintf(stderr, "Invalid segment size\n");
    exit(__LINE__);
  }

  if (port > 65331) {
    fprintf(stderr, "Invalid port number\n");
    exit(__LINE__);
  }
  if ((nworkerthreads < 1) || (nworkerthreads > 420)) {
    fprintf(stderr, "Invalid number of worker threads\n");
    exit(__LINE__);
  }
  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(__LINE__);
  }



  /* Initialize shared memory set-up here

  // Initialize server structure here
  */
  shm_stq = malloc(sizeof(steque_t));
  steque_init(shm_stq);
  sem_unlink("/sem_queue");
  sem_t *sem_queue = sem_open("/sem_queue", O_CREAT, 0666, 1);
  if (sem_queue == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < nsegments; i++) {
    // Create a shared memory object
    int *id = malloc(sizeof(int));
    *id = i;
    char shm_name[32] = {0};
    char sem_1_name[32] = {0};
    char sem_2_name[32] = {0};
    sprintf(shm_name, "/shm_%d", i);
    sprintf(sem_1_name, "/sem_1_%d", i);
    sprintf(sem_2_name, "/sem_2_%d", i);
    // create the shared memory
    shm_unlink(shm_name);
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
      perror("shm_open error");
      exit(EXIT_FAILURE);
    }
    // Set the size of the shared memory object
    if (ftruncate(fd, segsize) == -1) {
      perror("fgruncate error");
      exit(EXIT_FAILURE);
    }
    // create semaphores
    sem_unlink(sem_1_name);
    sem_t *sem_1 = sem_open(sem_1_name, O_CREAT, 0666, 0);
    if (sem_1 == SEM_FAILED) {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }
    sem_unlink(sem_2_name);
    sem_t *sem_2 = sem_open(sem_2_name, O_CREAT, 0666, 1);
    if (sem_2 == SEM_FAILED) {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }
//    int value;
//    sem_getvalue(sem_2, &value);
//    printf("webproxy -> %s value is: %d\n", sem_2_name, value);
    steque_enqueue(shm_stq, id);
  }



  gfserver_init(&gfs, nworkerthreads);

  // Set server options here
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 187);

  // Set up arguments for worker here
  for(int i = 0; i < nworkerthreads; i++) {
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, shm_stq);
  }

  // Invokethe framework - this is an infinite loop and will not return
  gfserver_serve(&gfs);

  // line never reached
  return -1;

}
