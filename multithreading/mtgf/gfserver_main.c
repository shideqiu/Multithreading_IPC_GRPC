
#include <stdlib.h>

#include "gfserver-student.h"

#define USAGE                                                               \
  "usage:\n"                                                                \
  "  gfserver_main [options]\n"                                             \
  "options:\n"                                                              \
  "  -h                  Show this help message.\n"                         \
  "  -t [nthreads]       Number of threads (Default: 20)\n"                 \
  "  -m [content_file]   Content file mapping keys to content files (Default: content.txt\n"      \
  "  -p [listen_port]    Listen port (Default: 10880)\n"                    \
  "  -d [delay]          Delay in content_get, default 0, range 0-5000000 " \
  "(microseconds)\n "

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"delay", required_argument, NULL, 'd'},
    {"nthreads", required_argument, NULL, 't'},
    {"content", required_argument, NULL, 'm'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

extern unsigned long int content_delay;

extern pthread_mutex_t m_server;
extern pthread_cond_t cond_server;
extern steque_t *queue;

extern gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void *arg);

static void _sig_handler(int signo) {
  if ((SIGINT == signo) || (SIGTERM == signo)) {
    exit(signo);
  }
}

void cleanup_threads() {
  content_destroy();
  steque_destroy(queue);
}

void *worker_thread(void *arg) {
  int *p = (int *)arg;
  int thread_id = *p;
  steque_item_info *item_info;
  while (1) {
    // lock
    pthread_mutex_lock(&m_server);
    while (steque_isempty(queue)) {
      pthread_cond_wait(&cond_server, &m_server);
    }
    item_info = steque_pop(queue);
    // unlock and signal
    pthread_mutex_unlock(&m_server);
    pthread_cond_signal(&cond_server);

    // check file path
    if (item_info == NULL) {
      printf("The item is null.\n");
      continue;
    }
    int file_descriptor = content_get(item_info->filepath);
    if (file_descriptor == -1) {
      gfs_sendheader(&item_info->context, GF_FILE_NOT_FOUND, 0);
      continue;
    }
    // get file info and send OK header
    struct stat buf;
    printf("This is worker thread %d\n", thread_id);
    if (fstat(file_descriptor, &buf) == -1) {
      perror("file status error");
      continue;
    }
    off_t file_length = buf.st_size;
    printf("The file size is %ld\n", file_length);
    gfs_sendheader(&item_info->context, GF_OK, file_length);
//    close(file_descriptor);
    // send the data
    char buffer[BUFSIZE] = {0};
    int bytes_sent = 0;
    while (bytes_sent < file_length) {
      int bytes_read = pread(file_descriptor, buffer, BUFSIZE, bytes_sent);
      gfs_send(&item_info->context, buffer, bytes_read);
      bytes_sent += bytes_read;
      memset(buffer, 0, sizeof(buffer));
    }
    free(item_info);
  }
}

void init_threads(size_t numthreads) {
  pthread_t tid[numthreads];
  for (int i = 0; i < numthreads; i++) {
    pthread_create(&tid[i], NULL, worker_thread, &i);
  }
}



/* Main ========================================================= */
int main(int argc, char **argv) {
  gfserver_t *gfs = NULL;
  int nthreads = 20;
  int option_char = 0;
  unsigned short port = 10880;
  char *content_map = "content.txt";

  setbuf(stdout, NULL);

  if (SIG_ERR == signal(SIGINT, _sig_handler)) {
    fprintf(stderr, "Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (SIG_ERR == signal(SIGTERM, _sig_handler)) {
    fprintf(stderr, "Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:d:rhm:t:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {
      case 'h':  /* help */
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      case 'p':  /* listen-port */
        port = atoi(optarg);
        break;
      case 't':  /* nthreads */
        nthreads = atoi(optarg);
        break;
      case 'm':  /* file-path */
        content_map = optarg;
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'd':  /* delay */
        content_delay = (unsigned long int)atoi(optarg);
        break;

    }
  }

  /* not useful, but it ensures the initial code builds without warnings */
  if (nthreads < 1) {
    nthreads = 1;
  }

  if (content_delay > 5000000) {
    fprintf(stderr, "Content delay must be less than 5000000 (microseconds)\n");
    exit(__LINE__);
  }

  content_init(content_map);

  /* Initialize thread management */
  queue = malloc(sizeof(steque_t));
  steque_init(queue);

  init_threads(nthreads);
  /*Initializing server*/
  gfs = gfserver_create();

  //Setting options
  gfserver_set_port(&gfs, port);
  gfserver_set_maxpending(&gfs, 21);
  gfserver_set_handler(&gfs, gfs_handler);
  gfserver_set_handlerarg(&gfs, NULL);  // doesn't have to be NULL!

  /*Loops forever*/
  gfserver_serve(&gfs);
  cleanup_threads();
}
