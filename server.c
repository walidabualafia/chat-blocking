/*
 * server.c - a chat server (and monitor) that uses pipes and sockets
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
#include <sys/wait.h>
#include <netdb.h>

// constants for pipe FDs
#define WFD 1
#define RFD 0

/*
 * monitor - provides a local chat window
 * @param srfd - server read file descriptor
 * @param swfd - server write file descriptor
 */
void monitor(int srfd, int swfd) {
  // Variables
  // read bytes, write bytes
  int rbytes, wbytes;
  // buffer string to hold the message 
  char buf[100];

  // keep reading while the user is typing, and there is no EOF
  while (1) {
    // handle read error
    if ((rbytes = read(srfd, buf, sizeof(buf))) == -1) {
      perror("read");
      exit(1);
    }
    // exit if we read an EOF
    if (rbytes == 0) {
      break;
    }
    // write to standard output
    if ((wbytes = write(STDOUT_FILENO, buf, rbytes)) == -1) {
      // handle write error
      perror("write");
      exit(1);
    }
    // print prompt
    printf("\n> ");
    fflush(stdout);
    // read from standard input
    if ((rbytes = read(STDIN_FILENO, buf, sizeof(buf))) == -1) {
      // handle read error
      perror("read");
      exit(1);
    }
    // exit if we read an EOF
    if (rbytes == 0) {
      break;
    }
    // write to server write file descriptor
    if ((wbytes = write(swfd, buf, rbytes)) == -1) {
      // handle write error
      perror("write");
      exit(1);
    }
  }
  printf("hanging up\n");
  exit(0);
}



/*
 * server - relays chat messages
 * @param mrfd - monitor read file descriptor
 * @param mwfd - monitor write file descriptor
 * @param portno - TCP port number to use for client connections
 */
void server(int mrfd, int mwfd, char *portno) {
  // Variables
  // socket fd, new_fd set by accept()
  int sfd, new_fd;
  // read bytes, write byes
  int rbytes, wbytes;
  // size of struct sockaddr_in
  socklen_t addr_size;
  // sockaddr
  struct sockaddr_in addr;
  struct addrinfo hints, *info, *p;
  // string buffer to hold the texts
  char buf[100];
  int val = 1;

  // set the addr family and the port to be def port
  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(portno));
  // memset hints and define the protocol family and the socket type
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  // get the addrinfo using getaddrinfo
  if ((getaddrinfo(NULL, portno, &hints, &info)) == -1) {
    // handle error
    perror("getaddrinfo");
    exit(1);
  }

  //addr.sin_addr.s_addr = INADDR_ANY;

  // loop over the incoming connections and accept the first incoming client on the port
  for (p = info; p != NULL; p = p->ai_next) {
    // create the socket and save its file descriptor in sfd
    if ((sfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
      // handle error
      perror("socket");
      exit(1);
    }
    
    // set the port to be reusable
    if ((setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val))) == -1) {
      // handle error
      close(sfd);
      perror("setsockopt");
      exit(1);
    }

    // bind all interfaces
    if ((bind(sfd, info->ai_addr, info->ai_addrlen)) == -1) {
      // handle error
      close(sfd);
      perror("bind");
      exit(1);
    }

    // set the addr to be reusable as the addr for the incoming connection
    if ((setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val))) == -1) {
      // handle error
      close(sfd);
      perror("setsockopt");
      exit(1);
    }

    break;
  }

  freeaddrinfo(info);

  if (p == NULL) {
    // handle error
    printf("server: failed to bind");
    exit(1);
  }

  // listen on the port
  if ((listen(sfd, 128)) == -1) {
    // handle error
    close(sfd);
    perror("listen");
    exit(1);
  }

  addr_size = sizeof(struct sockaddr_in); 
  // accept incoming connection
  new_fd = accept(sfd, (struct sockaddr *)&addr, (socklen_t *)&addr_size);
  if (new_fd == -1) {
    // handle error
    close(sfd);
    perror("accept");
    exit(1);
  }

  // Close the socket file descriptor
  close(sfd);

  // read from the new fd returned by the call to accept()
  //while ((rbytes = read(new_fd, buf, sizeof(buf))) > 0) {
  while (1) {
    if ((rbytes = read(new_fd, buf, sizeof(buf))) == -1) {
      // handle error
      perror("read");
      exit(1);
    }
    // if we read an EOF
    if (rbytes == 0) {
      break;
    }
    // write to the monitor write fd
    if ((wbytes = write(mwfd, buf, rbytes)) == -1) {
      // handle error
      perror("write");
      exit(1);
    } 
    // read from the monitor read fd
    if ((rbytes = read(mrfd, buf, sizeof(buf))) == -1) {
      // handle error
      perror("read");
      exit(1);
    }
    // if we read an EOF
    if (rbytes == 0) {
      break;
    }
    // write to the new_fd
    if ((wbytes = write(new_fd, buf, rbytes)) == -1) {
      // handle error
      perror("write");
      exit(1);
    }
  }

  close(new_fd);
  printf("hanging up\n");
  exit(0);
}



int main(int argc, char **argv) {
  // Variables
  // ret of getopt
  int opt;
  // default port
  char *defport = "5055";
  // pipes
  int pipefd1[2];
  int pipefd2[2];
  // child process id
  pid_t cpid;

  // while there are command line arguments to be consumed
  //    *command line args can be set with 2 flags:
  //    - p [port #]
  //    - h [help message]
  while ((opt = getopt(argc, argv, "hp:")) != -1) {
    switch (opt) {
    case 'h':
      printf("usage: ./client [-h] [-p port #]\n");
      printf("          -h - this help message \n");
      printf("          -p # - the port to use when connecting to the server\n");
      exit(0);
    case 'p':
      defport = optarg;
      break;
    }
  }

  // create pipe 1
  if ((pipe(pipefd1)) == -1) {
    // handle error
    perror("pipe 1");
    exit(1);
  }

  // create pipe 2
  if ((pipe(pipefd2)) == -1) {
    // handle error
    perror("pipe 2");
    exit(1);
  }

  // fork the process and handle error
  if ((cpid = fork()) == -1) {
    // handle error
    perror("fork");
    exit(1);
  }
  /* Child Process */
  if (cpid == 0) {
    close(pipefd1[1]);
    close(pipefd2[0]);
    monitor(pipefd1[0], pipefd2[1]);
    // when the child returns, hang up
    close(pipefd1[0]);
    close(pipefd2[1]);
  }
  /* Parent Process */
  else {
    close(pipefd1[0]);
    close(pipefd2[1]);
    server(pipefd2[0], pipefd1[1], defport); 
    // when the parent returns, hang up
    close(pipefd1[1]);
    close(pipefd2[0]);
    wait(NULL); /* Reap Child */
  } 

  return 0;

}
