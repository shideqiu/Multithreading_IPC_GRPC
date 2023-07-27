#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <getopt.h>
#include <netdb.h>

#define BUFSIZE 256

#define USAGE                                                        \
  "usage:\n"                                                         \
  "  echoserver [options]\n"                                         \
  "options:\n"                                                       \
  "  -p                  Port (Default: 10823)\n"                    \
  "  -m                  Maximum pending connections (default: 5)\n" \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"maxnpending", required_argument, NULL, 'm'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv) {
  int maxnpending = 5;
  int option_char;
  int portno = 10823; /* port to listen on */

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "hx:m:p:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'm':  // server
        maxnpending = atoi(optarg);
        break;
      case 'p':  // listen-port
        portno = atoi(optarg);
        break;
      case 'h':  // help
        fprintf(stdout, "%s ", USAGE);
        exit(0);
        break;
      default:
        fprintf(stderr, "%s ", USAGE);
        exit(1);
    }
  }

  setbuf(stdout, NULL);  // disable buffering

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }
  if (maxnpending < 1) {
    fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__,
            maxnpending);
    exit(1);
  }

  // Socket Code Here
  int sockfd, newsockfd, yes = 1, rv;
  char buffer[16] = {0};
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
  if (listen(sockfd, maxnpending) == -1) {
    perror("listen");
    exit(1);
  }

//    printf("Listen on %d", portno);
  while (1) {
    addr_size = sizeof their_addr;
    newsockfd = accept(sockfd, (struct sockaddr *) &their_addr, &addr_size);
    if (newsockfd == -1) {
      perror("accept");
      continue;
    }
//      printf("Listen on %d", portno);
    recv(newsockfd, buffer, 16, 0);
//      printf("%s", buffer);
//      printf("This is the message from client: %s \n", buffer);
//  write back to client.
    send(newsockfd, buffer, strlen(buffer), 0);
//    printf("the message is: %s", buffer);
    memset(buffer, 0, sizeof(buffer));
    close(newsockfd);
  }
  return 0;

}