#include "gfserver.h"
#include "cache-student.h"

#define BUFSIZE (824)

/*
 * Replace with your implementation
*/
ssize_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg) {
//  printf("handle with cache starts here\n");
  int return_value;
  sem_t *sem_queue = sem_open("/sem_queue", 0);
  steque_t *shm_stq = (steque_t *) arg;
  sem_wait(sem_queue);
  // check the queue
  while (steque_isempty(shm_stq) == 1) {
    sem_post(sem_queue);
    sleep(1);
    sem_wait(sem_queue);
  }
  int *id_ptr = steque_pop(shm_stq);
  int id = *id_ptr;
//  printf("the steque pop is: %d\n", id);
  sem_post(sem_queue);
//  int value;
//  sem_getvalue(sem_queue, &value);
//  printf("%s value is: %d\n", "sem_queue", value);
  // send a message queue
  // create a message queue
  mqd_t mqd;
  mqd = mq_open("/message_channel", O_RDWR | O_CREAT, 0666, NULL);
  if (mqd == (mqd_t) -1) {
    perror("mq_open");
    exit(1);
  }
//  printf("create a message\n");
  // send a message
  struct message msg_send;
  msg_send.id = id;
  strcpy(msg_send.path, path);
  if (mq_send(mqd, (const char *) &msg_send, sizeof(struct message), 0) == -1) {
//    printf("The problem is here\n");
    perror("mq_send");
    exit(1);
  }

  char sem_1_name[32] = {0};
  char sem_2_name[32] = {0};
  sprintf(sem_1_name, "/sem_1_%d", id);
  sprintf(sem_2_name, "/sem_2_%d", id);
//  printf("Semaphores names are %s and %s\n", sem_1_name, sem_2_name);
  sem_t *sem_1 = sem_open(sem_1_name, 0);
  sem_t *sem_2 = sem_open(sem_2_name, 0);

//  sem_post(sem_2);
//  int value;
//  sem_getvalue(sem_2, &value);
//  printf("%s value is: %d\n", sem_2_name, value);
  sem_wait(sem_1);
  char shm_name[32] = {0};
  sprintf(shm_name, "/shm_%d", id);
  int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
  struct message *msg = mmap(NULL, sizeof(struct message), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
//  int *new_id = malloc(sizeof(int));
//  *new_id = msg_2->id;
  if (msg->type == -1) {
    // File Not Found
//    printf("file is not found");
    memset(msg, 0, sizeof(struct message));
    munmap(msg, sizeof(struct message));
    sem_close(sem_1);
    sem_close(sem_2);

    steque_enqueue(shm_stq, id_ptr);
    return_value = gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    return return_value;
  }
  // Found file
  return_value = gfs_sendheader(ctx, GF_OK, msg->file_len);

  size_t bytes_transferred = 0;
  char buffer[BUFSIZ] = {0};
//  sem_post(sem_2);
//  printf("before gfs_send()\n");
  while (bytes_transferred < msg->file_len) {
    sem_wait(sem_1);
    memcpy(buffer, msg->content, msg->content_len);
//    printf("message: %s\n", buffer);
    gfs_send(ctx, buffer, msg->content_len);
    bytes_transferred += msg->content_len;
//    if (bytes_transferred == msg->file_len) {
//      break;
//    }
    memset(buffer, 0, sizeof(buffer));
    sem_post(sem_2);
  }
//  printf("finish here \n");
//  sem_wait(sem_1);
  memset(msg, 0, sizeof(struct message));
  munmap(msg, sizeof(struct message));
//  sem_post(sem_2);
  sem_close(sem_1);
  sem_close(sem_2);
  steque_enqueue(shm_stq, id_ptr);
  return return_value;
}

