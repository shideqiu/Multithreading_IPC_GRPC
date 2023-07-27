#include "gfclient-student.h"
// Modify this file to implement the interface specified in
// gfclient.h.
// Leo - create a gfcrequest_t struct and define the variable fgc_request with default scheme and method
// path need to get from other function
struct gfcrequest_t {
    // set
    unsigned short port;
    const char *path;
    const char *server;
    void *write_arg;
    // Leo - *write_func is a pointer, it point to a void function
    void (*write_func)(void *, size_t, void *);
    void *header_arg;
    void (*header_func)(void *, size_t, void *);

    // get
    gfstatus_t status;
    size_t file_length;
    size_t bytes_received;

};

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
  free(*gfr);
  *gfr = NULL;
}

gfcrequest_t *gfc_create() {
  // implemented
  // Leo - create a gfcrequest_t handle and assign it memory
  struct gfcrequest_t *gfcrequest_handle = malloc(sizeof *gfcrequest_handle);
  (*gfcrequest_handle).file_length = 0;
  (*gfcrequest_handle).bytes_received = 0;
  (*gfcrequest_handle).status = GF_INVALID;
  return gfcrequest_handle;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
  // implemented
  return (*gfr)->bytes_received;
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
  // implemented
  return (*gfr)->file_length;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
  // implemented
  return (*gfr)->status;
}

void gfc_global_init() {}

void gfc_global_cleanup() {}

int gfc_perform(gfcrequest_t **gfr) {
  // implemented
  char buffer[BUFSIZE] = {0};
  int sockfd, bytes_received;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;  // don't care ipv4 or ipv6; AF stands for address family
  hints.ai_socktype = SOCK_STREAM;  // TCP stream socket
  hints.ai_protocol = 0;
  char port[10];
  sprintf(port, "%d", (*gfr)->port);
  if ((rv = getaddrinfo((*gfr)->server, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));

    exit(1);
  }
  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      (*gfr)->status = GF_ERROR;
      close(sockfd);
      perror("client: connect");
      continue;
    }
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  freeaddrinfo(servinfo);

//  printf("------------Send the GETFILE Request-----------\n");
  // send the GETFILE request
  char msg[100]={0};
  sprintf(msg, "GETFILE GET %s\r\n\r\n", (*gfr)->path);
  if ((send(sockfd, msg, strlen(msg), 0)) == -1) {
    (*gfr)->status = GF_ERROR;
    perror("send");
  }
//  printf("--------------Receive from the server---------------\n");
  int first_buffer = 0;
  int data_remained = 100;
  while (data_remained != 0) {
    bytes_received = recv(sockfd, buffer, BUFSIZE, 0);
    if (bytes_received == 0) {
      (*gfr)->bytes_received = 0;
      return -1;
    }
//    buffer[bytes_received] = '\0'; we don't need this because of the write_func.
    if (first_buffer == 0) {
      char *token, *scheme, *status, *length, *content;
      // parse the header to get the scheme
//      printf("aa%saa\n", buffer);
      token = strtok(buffer, " ");
      scheme = token;
//      printf("----------This is the scheme: %s--------------\n", scheme);
      // parse the header to get the status
      token = strtok(NULL, " ");
      status = token;
//      printf("----------This is the status: aaa %s aaa--------------\n", status);
//      printf("%ld", strlen(status));

      // check the header
      if (strcmp(scheme, "GETFILE") != 0) {
        (*gfr)->status = GF_INVALID;
        return -1;
      }
      if (strcmp(status, "OK") == 0) {
        (*gfr)->status = GF_OK;
      } else {
        strtok(status, "\r\n\r\n");
        if (strcmp(status, "FILE_NOT_FOUND") == 0) {
          (*gfr)->status = GF_FILE_NOT_FOUND;
          printf("file is not found");
        }
        if (strcmp(status, "ERROR") == 0) {
          (*gfr)->status = GF_ERROR;
          printf("file is error");

        }
        if (strcmp(status, "INVALID") == 0) {
          (*gfr)->status = GF_INVALID;
          printf("file is invalid");
          return -1;
        }
        printf("-------status: %u\n", (*gfr)->status);
        if ((*gfr)->status == 3) {
          // sometimes the status is null. Thus, the default invalid is used by not checked. We double check it here and return -1
          return -1;
        }
        return 0;
      }
      // parse the header to get the file length
      token = strtok(NULL, "\r\n\r\n");
      length = token;
      printf("----------This is the file size: %s--------------\n", length);

      // update the file length if it's unknown
      if ((*gfr)->file_length == 0) {
        sscanf(length, "%zu", &(*gfr)->file_length);
      }
      token = strtok(NULL, "\n\r\n");
      content = token;
//      printf("This is the header content: %s\n", content);
//      printf("The size of the first content is: %ld\n", strlen(content));
//      printf("The byte received is: %d\n", bytes_received);
//      printf("%ld %ld %ld\n", strlen(scheme), strlen(status), strlen(length));
      int response_size = bytes_received - strlen(scheme) - strlen(status) - strlen(length) - 6;
      printf("bytes_received: %d, length: %lu", bytes_received, strlen(length));
      printf("The response size is %d\n", response_size);
      printf("scheme: %s, status: %s, length: %s", scheme, status, length);

      data_remained = (*gfr)->file_length - response_size;
      first_buffer = 1;
      (*gfr)->bytes_received = response_size;
      if (response_size > 0) {
        (*gfr)->write_func(content, response_size, (*gfr)->write_arg);
      }
//      printf("--------------Successfully write func----------------\n");
    } else {
//      char *content = buffer;
//      printf("The bytes received is: %d\n", bytes_received);
      data_remained -= bytes_received;
//      printf("---------data_remained: %d--------------\n", data_remained);
      (*gfr)->bytes_received += bytes_received;
      (*gfr)->write_func(buffer, bytes_received, (*gfr)->write_arg);
    }

    memset(buffer, 0, sizeof(buffer));
  }
  close(sockfd);
//  printf("-----------Finish receiving-----------\n");
  if ((*gfr)->status == 3) {
    return -1;
  }
  return 0;
}



void gfc_set_server(gfcrequest_t **gfr, const char *server) {
  // Leo - there are two ways to access struct variable
  // No1. (*(*gfr)).server = server;
  (*gfr)->server = server;
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {(*gfr)->port = port;}

void gfc_set_path(gfcrequest_t **gfr, const char *path) {(*gfr)->path = path;}

void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg) {(*gfr)->header_arg = headerarg;}

void gfc_set_headerfunc(gfcrequest_t **gfr,
                        void (*headerfunc)(void *, size_t, void *)) {(*gfr)->header_func = headerfunc;}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {(*gfr)->write_arg = writearg;}

void gfc_set_writefunc(gfcrequest_t **gfr,
                       void (*writefunc)(void *, size_t, void *)) {(*gfr)->write_func = writefunc;}

const char *gfc_strstatus(gfstatus_t status) {
  const char *strstatus = "UNKNOWN";

  switch (status) {

    case GF_ERROR: {
      strstatus = "ERROR";
    } break;

    case GF_OK: {
      strstatus = "OK";
    } break;


    case GF_INVALID: {
      strstatus = "INVALID";
    } break;


    case GF_FILE_NOT_FOUND: {
      strstatus = "FILE_NOT_FOUND";
    } break;
  }

  return strstatus;
}
