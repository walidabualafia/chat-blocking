/*
 * client.c - a turn taking chat client
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

// constants for pipe FDs
#define WFD 1
#define RFD 0

int main(int argc, char **argv) {
  // Variables
  // socket fd
  int sfd;
  // getopt value
  int opt;
  // read and write bytes
  int rbytes, wbytes;
  // sockaddr
  struct sockaddr_in addr;
  struct addrinfo hints, *info, *p;
  // default port
  char *defport = "5055";
  // string buffer to hold the texts
  char buf[100];

  // while there are command line arguments to be consumed
  //    *command line args can be set with 2 flags:
  //    - p [port #]
  //    - h [help message]
  while ((opt = getopt(argc, argv, "hp:")) != -1) {
    switch (opt) {
    case 'h':
      printf("usage: ./client [-h] [-p port #]\n");
      printf("         -h - this help message\n");
      printf("         -p # - the port to use when connecting to the server\n");
      exit(0);
    case 'p':
      defport = optarg;
      break;
    }
  }
  
  // set the addr family and the port to be def port
  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(defport));
  // memset hints and define the protocol family and the socket type
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype = SOCK_STREAM; 
  // get the addrinfo using getaddrinfo
  if (getaddrinfo("login02", defport, &hints, &info) != 0) {
    // handle error
    perror("getaddrinfo");
    exit(1);
  }

  // loop over the incoming connections and accept the first incoming client on the port
  for (p = info; p != NULL; p = p->ai_next) {
    // create the socket and save its file descriptor in sfd
    if ((sfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      // handle error
      perror("socket");
      exit(1);
    }

    // connect to the server listening on the port
    if (connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
      // handle error
      close(sfd);
      perror("connect");
      exit(1);
    }
    break;
  }

  freeaddrinfo(info);

  if (p == NULL) {
    // handle error
    printf("client: failed to connect");
    exit(1);
  }

  // print the connect message if the server connects
  printf("connected to server...\n\n> ");
  fflush(stdout);
  
  // read the text from the user on stdin 
  while (1) {
    if ((rbytes = read(STDIN_FILENO, buf, sizeof(buf))) == -1) {
      // handle error
      perror("read");
      exit(1);
    }
    // if we read an EOF
    if (rbytes == 0) {
      break;
    }
    // write the text to the relay server on the socket 
    if ((wbytes = write(sfd, buf, rbytes)) == -1) {
      // handle error
      perror("write");
      exit(1);
    } 

    // read the reply from the relay server on the socket
    if ((rbytes = read(sfd, buf, sizeof(buf))) == -1) {
      // handle error
      perror("read");
      exit(1);
    }
    // if we read an EOF
    if (rbytes == 0) {
      break;
    }
    // write the reply from the relay server to the user on stdout
    if ((wbytes = write(STDOUT_FILENO, buf, rbytes)) == -1) {
      // handle error
      perror("write");
      exit(1);
    }

    // print the prompt
    printf("\n> ");
    fflush(stdout);
  }

  /* Cleanup Sequence */
  close(sfd);
  printf("hanging up\n");
  exit(0);

}
