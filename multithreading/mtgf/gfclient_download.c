#include <stdlib.h>

#include "gfclient-student.h"

#define MAX_THREADS 1572
#define PATH_BUFFER_SIZE 1536

#define USAGE                                                    \
  "usage:\n"                                                     \
  "  webclient [options]\n"                                      \
  "options:\n"                                                   \
  "  -h                  Show this help message\n"               \
  "  -s [server_addr]    Server address (Default: 127.0.0.1)\n"  \
  "  -p [server_port]    Server port (Default: 10880)\n"         \
  "  -w [workload_path]  Path to workload file (Default: workload.txt)\n"\
  "  -t [nthreads]       Number of threads (Default 16 Max: 1572)\n"       \
  "  -r [num_requests]   Request download total (Default: 12)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
char *server = "localhost";
char *workload_path = "workload.txt";
int option_char = 0;
unsigned short port = 10880;
steque_t *queue;
pthread_cond_t cond_client = PTHREAD_COND_INITIALIZER;
pthread_mutex_t m_client = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t m_request = PTHREAD_MUTEX_INITIALIZER;
int returncode = 0;
int nthreads = 120;
int nrequests = 5;
//int requests_left = -1;

static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"nrequests", required_argument, NULL, 'r'},
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"workload-path", required_argument, NULL, 'w'},
    {"nthreads", required_argument, NULL, 't'},
    {NULL, 0, NULL, 0}};

static void Usage() { fprintf(stderr, "%s", USAGE); }

static void localPath(char *req_path, char *local_path) {
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE *openFile(char *path) {
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while (NULL != (cur = strchr(prev + 1, '/'))) {
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)) {
      if (errno != EEXIST) {
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if (NULL == (ans = fopen(&path[0], "w"))) {
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void *data, size_t data_len, void *arg) {
  FILE *file = (FILE *)arg;
  fwrite(data, 1, data_len, file);
}

// worker thread
void *worker_thread(void *arg) {
//  int *p = (int *)arg;
//  int thread_id = *p;
//  printf("This is worker thread");

  char local_path[PATH_BUFFER_SIZE];
  char req_path_worker[PATH_BUFFER_SIZE];
  int done = -1;

//  printf("--------queue status: %d\n", steque_isempty(queue));
  while (1) {
//    pthread_mutex_lock(&m_request);
//    int left = requests_left;
//    requests_left--;
////    printf("The worker stuck at the first request lock\n");
//    pthread_mutex_unlock(&m_request);
//    printf("The worker pass at the first request lock\n");

//    if (left <= 0) break;
    //lock
    pthread_mutex_lock(&m_client);
    while (steque_isempty(queue)) {
      pthread_cond_wait(&cond_client, &m_client);
    }
//    printf("------------pop start-----------");
    strcpy(req_path_worker, steque_pop(queue));
//    printf("the queue size is: %d\n", steque_size(queue));
//    printf("The path is %s\n", req_path_worker);
//    printf("id is %d, req path is %s\n", thread_id, req_path);
    if (strcmp(req_path_worker, "Done") == 0) {
      steque_enqueue(queue, "Done");
      done = 1;
//      printf("Done\n");
    }
//    printf("The worker stuck at the client lock\n");


    // unlock
    pthread_mutex_unlock(&m_client);
//    printf("The worker pass at the client lock\n");

    pthread_cond_broadcast(&cond_client);
    if (done == 1) break;
    // lock request_left and update
//    pthread_mutex_lock(&m_request);
//    requests_left--;
////    printf("The worker stuck at the second request lock\n");
//
//    pthread_mutex_unlock(&m_request);
//    printf("The worker pass at the second request lock\n");

    localPath(req_path_worker, local_path);
    FILE *file = NULL;
    file = openFile(local_path);

    gfcrequest_t *gfr = NULL;
    gfr = gfc_create();
    gfc_set_path(&gfr, req_path_worker);

    gfc_set_port(&gfr, port);
    gfc_set_server(&gfr, server);
    gfc_set_writearg(&gfr, file);
    gfc_set_writefunc(&gfr, writecb);

//    fprintf(stdout, "Requesting %s%s\n", server, req_path_worker);

    if (0 > (returncode = gfc_perform(&gfr))) {
      fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
      fclose(file);
      if (0 > unlink(local_path))
        fprintf(stderr, "warning: unlink failed on %s\n", local_path);
    } else {
      fclose(file);
    }

    if (gfc_get_status(&gfr) != GF_OK) {
      if (0 > unlink(local_path)) {
        fprintf(stderr, "warning: unlink failed on %s\n", local_path);
      }
    }

//    fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(&gfr)));
//    fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(&gfr),
//            gfc_get_filelen(&gfr));

    gfc_cleanup(&gfr);
  }
  return 0;
}


/* Main ========================================================= */
int main(int argc, char **argv) {
  /* COMMAND LINE OPTIONS ============================================= */
  setbuf(stdout, NULL);  // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:n:hs:t:r:w:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {
      case 'r': // nrequests
        nrequests = atoi(optarg);
        break;
      case 's':  // server
        server = optarg;
        break;
     case 't':  // nthreads
        nthreads = atoi(optarg);
        break;
      default:
        Usage();
        exit(1);
      case 'p':  // port
        port = atoi(optarg);
        break;
      case 'n':
        break;
      case 'w':  // workload-path
        workload_path = optarg;
        break;

      case 'h':  // help
        Usage();
        exit(0);
    }
  }

  if (EXIT_SUCCESS != workload_init(workload_path)) {
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }
  if (port > 65331) {
    fprintf(stderr, "Invalid port number\n");
    exit(EXIT_FAILURE);
  }
  if (nthreads < 1 || nthreads > MAX_THREADS) {
    fprintf(stderr, "Invalid amount of threads\n");
    exit(EXIT_FAILURE);
  }
  gfc_global_init();

  // add your threadpool creation here
//  requests_left = nrequests;
//  char req_path[PATH_BUFFER_SIZE] = {0};
  char *req_path = NULL;
  queue = malloc(sizeof(steque_t));
  steque_init(queue);
  pthread_t tid[nthreads];
  for (int i = 0; i < nthreads; i++) {
    pthread_create(&tid[i], NULL, worker_thread, &i);
  }

  /* Build your queue of requests here */
  for (int i = 0; i < nrequests; i++) {
    /* Note that when you have a worker thread pool, you will need to move this
     * logic into the worker threads */

    // lock
    pthread_mutex_lock(&m_client);
    req_path = workload_get_path();
    // strcpy is a deep copy, cannot use here.
//    strcpy(req_path, workload_get_path())
    if (strlen(req_path) > PATH_BUFFER_SIZE) {
      fprintf(stderr, "Request path exceeded maximum of %d characters\n.", PATH_BUFFER_SIZE);
      exit(EXIT_FAILURE);
    }

    steque_enqueue(queue, req_path);
//    printf("Boss is requesting path: %s\n", req_path);
    // unlock and signal
    pthread_mutex_unlock(&m_client);
    pthread_cond_signal(&cond_client);
  }
  pthread_mutex_lock(&m_client);
  steque_enqueue(queue, "Done");
  pthread_mutex_unlock(&m_client);
  pthread_cond_signal(&cond_client);
//  printf("equeue is done.\n");
  for (int i = 0; i < nthreads; i++) {
    pthread_join(tid[i], NULL);
//    printf("The worker %d is finished.\n", i);
  }

    /*
     * note that when you move the above logic into your worker thread, you will
     * need to coordinate with the boss thread here to effect a clean shutdown.
     */
//  free(tid);
  steque_destroy(queue);
  gfc_global_cleanup();  /* use for any global cleanup for AFTER your thread
                          pool has terminated. */

  return 0;
}
