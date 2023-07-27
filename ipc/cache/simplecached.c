#include <stdio.h>
#include <unistd.h>
#include <printf.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>

#include "cache-student.h"
#include "shm_channel.h"
#include "simplecache.h"
#include "gfserver.h"

// CACHE_FAILURE
#if !defined(CACHE_FAILURE)
    #define CACHE_FAILURE (-1)
#endif

#define MAX_CACHE_REQUEST_LEN 6200
#define MAX_SIMPLE_CACHE_QUEUE_SIZE 824
mqd_t mqd;
unsigned long int cache_delay;

static void _sig_handler(int signo){
	if (signo == SIGTERM || signo == SIGINT){
		/*you should do IPC cleanup here*/
      printf("clean the message queue here\n");
      mq_close(mqd);
      mq_unlink("/message_channel");
      exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default is 42, Range is 1-235711)\n"      \
"  -d [delay]          Delay in simplecache_get (Default is 0, Range is 0-2500000 (microseconds)\n "	\
"  -h                  Show this help message\n"

//OPTIONS
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"help",               no_argument,            NULL,           'h'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {"delay", 			 required_argument,		 NULL, 			 'd'}, // delay.
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

void *worker_thread(void *arg) {
// receive the message from the message queue
  // get attr of the message queue
  mqd_t *mqd_ptr = (mqd_t *) arg;
  mqd_t mqd_id = *mqd_ptr;
  struct mq_attr attr;
  if (mq_getattr(mqd_id, &attr) == -1) {
    perror("mq_getattr");
    exit(1);
  }
//  printf("get attr\n");

  while (1) {
//    printf("receive message queue: %d\n", mqd_id);
//    printf("current message is %ld\n", attr.mq_curmsgs);
    char *buffer = calloc(attr.mq_msgsize, 1);
    ssize_t numRead = mq_receive(mqd_id, buffer, attr.mq_msgsize, 0);
    if (numRead == -1) {

//      printf("mq receive error: %d\n", mqd_id);
//      printf("mq msgsize: %ld\n", attr.mq_msgsize);
//      printf("mq current message: %ld\n", attr.mq_curmsgs);
      perror("mq_receive");
      exit(1);
    }
    struct message *msg = (struct message *) buffer;

//    printf("The message id is: %d\n", msg->id);
//    printf("The messgae is received: %s\n", msg->path);

    // get the semaphore for the segment of shared memory
    char sem_1_name[32];
    char sem_2_name[32];
    sprintf(sem_1_name, "/sem_1_%d", msg->id);
    sprintf(sem_2_name, "/sem_2_%d", msg->id);
    sem_t *sem_1 = sem_open(sem_1_name, 0);
    sem_t *sem_2 = sem_open(sem_2_name, 0);
    if (sem_1 == SEM_FAILED || sem_2 == SEM_FAILED) {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }

//    int value;
//    sem_getvalue(sem_2, &value);
//    printf("%s value before: %d\n", sem_2_name, value);
//    sem_wait(sem_2);
//    sem_getvalue(sem_2, &value);
//    printf("%s value after: %d\n", sem_2_name, value);
    char shm_name[32];
    sprintf(shm_name, "/shm_%d", msg->id);
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);

    int fd = simplecache_get(msg->path);
    // File Not Found
    printf("Retrieve %s\n", msg->path);
    if (fd == -1) {
//      printf("File is not found\n");
      msg = mmap(NULL, sizeof(*msg), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
      msg->type = -1;
      sem_post(sem_1);
      sem_close(sem_1);
      sem_close(sem_2);
      continue;
    }
    // Found file
//    printf("Semaphores names are %s and %s\n", sem_1_name, sem_2_name);

    char file_buffer[MAX_SIMPLE_CACHE_QUEUE_SIZE] = {0};
    int offset = 0;
    ssize_t bytes_read = 0;
    struct stat file_stats;
    if (fstat(fd, &file_stats) < 0) {
      return (void *) SERVER_FAILURE;
    }
//    printf("file size is: %ld\n", file_stats.st_size);
    int size = file_stats.st_size;
    msg = mmap(NULL, sizeof(*msg), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    msg->type = 1;
    msg->file_len = size;

    sem_post(sem_1);
//    sem_getvalue(sem_2, &value);
//    printf("%s value is %d\n", sem_2_name, value);
//    sem_wait(sem_2);

    while ((bytes_read = pread(fd, file_buffer, MAX_SIMPLE_CACHE_QUEUE_SIZE, offset)) > 0) {
      sem_wait(sem_2);
      memset(msg->content, 0, MAX_SIMPLE_CACHE_QUEUE_SIZE);
      memcpy(msg->content, file_buffer, bytes_read);
//      printf("message: %s\n", file_buffer);
      msg->content_len = bytes_read;
      offset += bytes_read;
      memset(file_buffer, 0, MAX_SIMPLE_CACHE_QUEUE_SIZE);
//      printf("reset buffer\n");
//      printf("The bytes_read is: %ld\n", bytes_read);
      sem_post(sem_1);
    }
//    printf("The read is all done\n");
//    sem_post(sem_1);
    sem_close(sem_1);
    sem_close(sem_2);
    free(buffer);
    buffer = NULL;
  }
}

int main(int argc, char **argv) {
	int nthreads = 11;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "d:ic:hlt:x", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			default:
				Usage();
				exit(1);
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;
            case 'c': //cache directory
				cachedir = optarg;
				break;
            case 'd':
				cache_delay = (unsigned long int) atoi(optarg);
				break;
			case 'i': // server side usage
			case 'o': // do not modify
			case 'a': // experimental
				break;
		}
	}

	if (cache_delay > 2500001) {
		fprintf(stderr, "Cache delay must be less than 2500001 (us)\n");
		exit(__LINE__);
	}

	if ((nthreads>211804) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads must be in between 1-211804\n");
		exit(__LINE__);
	}
	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}
	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}
	/*Initialize cache*/
	simplecache_init(cachedir);

    // receive the message queue
    // open the message queue

    mqd = mq_open("/message_channel", O_RDWR | O_CREAT, 0666, NULL);
    if (mqd == (mqd_t) -1) {
      perror("mq_open");
      exit(1);
    }

    void *mqd_ptr = &mqd;

	// Cache should go here
    // Initialize thread management
    pthread_t tid[nthreads];
    for (int i = 0; i < nthreads; i++) {
      pthread_create(&tid[i], NULL, worker_thread, mqd_ptr);
    }

    // Join the thread
    for (int i = 0; i < nthreads; i++) {
      printf("Thread %d close\n", i);
      pthread_join(tid[i], NULL);
      free(&tid[i]);
    }
	// Line never reached
	return 0;
}
