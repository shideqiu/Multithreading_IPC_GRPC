#include "gfserver-student.h"

// Modify this file to implement the interface specified in
 // gfserver.h.
struct gfserver_t {
    // set
    unsigned short port;
    int max_npending;
    void *arg;
    gfh_error_t (*handler)(gfcontext_t **, const char *, void*);

    // get
    gfstatus_t status;

};

struct gfcontext_t {
  int clientfd;
  gfstatus_t status;
  char *filepath;
};

void gfs_abort(gfcontext_t **ctx){
  close((*ctx)->clientfd);
  free(*ctx);
  *ctx = NULL;
}

gfserver_t* gfserver_create(){
    // not yet implemented
  struct gfserver_t *gfs_handler = malloc(sizeof *gfs_handler);

  return gfs_handler;

}

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len){
    // not yet implemented
//    printf("----------The file sent is: %p\n", data);
//    printf("----------The file length is: %ld\n", len);
    return send((*ctx)->clientfd, data, len, 0);
}

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len){
    // not yet implemented
    char header[1024] = {0};
    switch (status) {
      case GF_INVALID: {
        sprintf(header, "GETFILE INVALID\r\n\r\n");
        break;
      }
      case GF_ERROR: {
        sprintf(header, "GETFILE ERROR\r\n\r\n");
        break;
      }
      case GF_FILE_NOT_FOUND: {
        sprintf(header, "GETFILE FILE_NOT_FOUND\r\n\r\n");
        break;
      }
      case GF_OK: {
        sprintf(header, "GETFILE OK %zu\r\n\r\n", file_len);
        break;
      }
    }
//    printf("The status is: %d\n", status);
    printf("The header sending to client is: %s\n", header);

    return send((*ctx)->clientfd, header, strlen(header), 0);
}

void gfserver_set_handlerarg(gfserver_t **gfs, void* arg){(*gfs)->arg = arg;}




void gfserver_serve(gfserver_t **gfs){

  char buffer[BUFSIZE] = {0};
  char header[BUFSIZE] = {0};
  int sockfd, new_fd, yes=1, bytes_received = 0, all_bytes_received = 0;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;  // client's address information
  socklen_t addr_size;
  int rv;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;  // don't care ipv4 or ipv6; AF stands for address family
  hints.ai_socktype = SOCK_STREAM;  // TCP stream socket
  hints.ai_protocol = 0;
  hints.ai_flags = AI_PASSIVE;  // use my IP
  char port[10];
  sprintf(port, "%d", (*gfs)->port);
  if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
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

    // allow to reuse the socket port if in use
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
      perror("setsockopt");
      exit(1);
    }

    // bind it to the port we passed in to getaddrinfo()
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }
    break;
  }

  // all done with this structure
  freeaddrinfo(servinfo);

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  // listen
  if (listen(sockfd, (*gfs)->max_npending) == -1) {
    perror("listen");
    exit(1);
  }

//  printf("server: waiting for connections...\n");
  // accept, waiting for connections
//  int client_header = 1;

  while (1) {  // main accept() loop
    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }
    gfcontext_t *gfc = malloc(sizeof *gfc);
    gfc->clientfd = new_fd;
//    printf("-------Server receive the message and check the header from the client.---------\n");
    // receive and check the header from the client
    while (strstr(header, "\r\n\r\n") == NULL) {
      bytes_received = recv(new_fd, buffer, BUFSIZE - 1, 0);
      buffer[bytes_received] = '\0';
      strcpy(header + all_bytes_received, buffer);
      all_bytes_received += bytes_received;
//      printf("%s\n", buffer);
//      printf("%s\n", header);
//      printf("byte received: %d\n", bytes_received);
      memset(buffer, 0, sizeof(buffer));
      }

//    printf("----------all buffer received: %s-----------\n", header);
    // check the request header format
    int send = 0;
    char *token, *scheme, *method, *path;
    // parse the header to get the scheme
    token = strtok(header, " ");
    scheme = token;
//    printf("----------This is the scheme: %s--------------\n", scheme);
    if (strcmp(scheme, "GETFILE") != 0) {
      (*gfs)->status = GF_INVALID;
      send = -1;
    }
    // parse the header to get the method
    token = strtok(NULL, " ");
    method = token;
//    printf("----------This is the method: %s--------------\n", method);

    if (strcmp(method, "GET") != 0) {
      (*gfs)->status = GF_INVALID;
      send = -1;
    }
    // parse the header to get the path
    token = strtok(NULL, "\r\n\r\n");
    path = token;
    printf("----------This is the path: %s--------------\n", path);
    if (path[0] != '/') {
      (*gfs)->status = GF_INVALID;
      send = -1;
    }
    if (send ==-1) {
//      printf("----------The header sent from the server is bad-------------\n");
      gfs_sendheader(&gfc, GF_INVALID, -1);
    } else {
//      printf("----------The header sent from the server is good-------------\n");
      gfc->filepath = path;
      (*gfs)->handler(&gfc, gfc->filepath, (*gfs)->arg);
    }
  }
}

void gfserver_set_handler(gfserver_t **gfs,
                          gfh_error_t (*handler)(gfcontext_t **, const char *, void*)){(*gfs)->handler = handler;}

void gfserver_set_port(gfserver_t **gfs, unsigned short port){(*gfs)->port = port;}

void gfserver_set_maxpending(gfserver_t **gfs, int max_npending){(*gfs)->max_npending = max_npending;}



