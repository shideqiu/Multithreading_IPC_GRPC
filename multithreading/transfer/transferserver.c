#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 512

#define USAGE                                            \
  "usage:\n"                                             \
  "  transferserver [options]\n"                         \
  "options:\n"                                           \
  "  -h                  Show this help message\n"       \
  "  -f                  Filename (Default: 6200.txt)\n" \
  "  -p                  Port (Default: 10823)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"filename", required_argument, NULL, 'f'},
    {NULL, 0, NULL, 0}};
int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    printf("Sent\n");
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
        printf("Sent part\n");
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int main(int argc, char **argv) {
  char *filename = "6200.txt"; // file to transfer 
  int portno = 10823;          // port to listen on 
  int option_char;

  setbuf(stdout, NULL);  // disable buffering

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "hf:xp:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'p':  // listen-port
        portno = atoi(optarg);
        break;
      case 'h':  // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'f':  // file to transfer
        filename = optarg;
        break;
    }
  }

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  if (NULL == filename) {
    fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Socket Code Here */
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
      printf("The file doesn't exist.");
      exit(0);
  }
  int sockfd, newsockfd, yes = 1, rv, bytes_received;
  char buffer[BUFSIZE+1] = {0};
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;  // don't care ipv4 or ipv6; AF stands for address family
  hints.ai_socktype = SOCK_STREAM;  // TCP stream socket
  hints.ai_protocol = 0;
  hints.ai_flags = AI_PASSIVE;  // use my IP
  char port[10];
  sprintf(port, "%d", portno);
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
  freeaddrinfo(servinfo);

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }
  // listen
  if (listen(sockfd, 5) == -1) {
    perror("listen");
    exit(1);
  }

    while (1) {
      addr_size = sizeof their_addr;
      newsockfd = accept(sockfd, (struct sockaddr *) &their_addr, &addr_size);//    memset(buffer, 0, sizeof(buffer));
      if (newsockfd == -1) {
        perror("accept");
        continue;
      }
        while ((bytes_received = fread(buffer, 1, BUFSIZE, fp)) > 0) {
//        printf("%s\n", buffer);
            send(newsockfd, buffer, strlen(buffer), 0);
//        printf("%lu\n", sizeof(buffer));
//        need to reset the buffer to 0. Otherwise if the buffer size is larget than the remaining file, it will transfer the reamining text left in the buffer.
            memset(buffer, 0, sizeof(buffer));
//        ssize_t sent = write(newsockfd, buffer, BUFSIZE);
//        printf("%d\n", n);
//        printf("%s\n", buffer);
//        printf("%d, %d\n", n, BUFSIZE);
//        while (sent != BUFSIZE) {
//            sent = write(newsockfd, buffer, BUFSIZE);
//        }
        }
        close(newsockfd);
    }
    return 0;
}
