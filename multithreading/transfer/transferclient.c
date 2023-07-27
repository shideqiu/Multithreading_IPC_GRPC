#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#define BUFSIZE 512

#define USAGE                                                \
  "usage:\n"                                                 \
  "  transferclient [options]\n"                             \
  "options:\n"                                               \
  "  -h                  Show this help message\n"           \
  "  -p                  Port (Default: 10823)\n"            \
  "  -s                  Server (Default: localhost)\n"      \
  "  -o                  Output file (Default cs6200.txt)\n" 

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {{"server", required_argument, NULL, 's'},
                                       {"output", required_argument, NULL, 'o'},
                                       {"port", required_argument, NULL, 'p'},
                                       {"help", no_argument, NULL, 'h'},
                                       {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv) {
  unsigned short portno = 10823;
  int option_char = 0;
  char *hostname = "localhost";
  char *filename = "cs6200.txt";

  setbuf(stdout, NULL);

  /* Parse and set command line arguments */ 
  while ((option_char =
              getopt_long(argc, argv, "xp:s:h:o:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 's':  // server
        hostname = optarg;
        break;
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
      case 'o':  // filename
        filename = optarg;
        break;
    }
  }

  if (NULL == hostname) {
    fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
    exit(1);
  }

  if (NULL == filename) {
    fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
    exit(1);
  }

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  // Socket Code Here
  FILE *fp = fopen(filename, "wb");
  int sockfd, rv;
  char buffer[BUFSIZE+1] = {0};
  struct addrinfo hints, *servinfo, *p;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  char port[10];
  sprintf(port, "%d", portno);
  if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
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
  int n;
//  printf("%lu\n", strlen(buffer));
//  printf("while starts");
//    n = recv(sockfd, buffer, BUFSIZE, 0);
//    printf("%s\n", buffer);
//    bzero(buffer, BUFSIZE+1);
//    n = recv(sockfd, buffer, BUFSIZE, 0);
//    printf("%c\n", buffer[0]);
//    printf("%s\n", buffer);
  while ((n = recv(sockfd, buffer, BUFSIZE, 0)) > 0) {
//      printf("%d, %lu\n", n, strlen(buffer));
//      buffer[strlen(buffer)] = '\0';
//      if (strlen(buffer) < BUFSIZE) break;
//      buffer[n+1] = '\n';
//        int len = 100;
//        for (len; len <= n; len++) {
////            printf("%d", len);
//            buffer[len] = '\0';
////            printf("%c", buffer[len]);
//        }
//        printf("\n");
//        printf("%d, %d\n", len, n);
//        printf("%c\n", buffer[len-5]);
//      printf("%c\n", buffer[len-4]);
//      printf("%c\n", buffer[len-3]);
//      printf("%c\n", buffer[len-2]);
//      printf("%c\n", buffer[len-1]);
//      printf("len: %c\n", buffer[len]);
//      printf("%c\n", buffer[len+1]);
//      printf("%c\n", buffer[len+2]);
//      printf("%c\n", buffer[len+3]);
//      printf("%c", buffer[n-5]);
//      printf("%c", buffer[n-4]);
//      printf("%c", buffer[n-3]);
//      printf("%c", buffer[n-2]);
//      printf("%c", buffer[n-1]);
//        printf("n: %c", buffer[n]);
//      printf("%c", buffer[n+1]);
//      printf("%c", buffer[n+2]);
//      printf("%c\n", buffer[n+3]);

//        printf("%s", buffer);
      fwrite(buffer, strlen(buffer), 1, fp);
      memset(buffer, 0, sizeof(buffer));

  }
  fclose(fp);
//  printf("%d", n);
    return 0;
}
