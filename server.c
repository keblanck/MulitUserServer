/*
 * Kathleen Blanck
 * 10/20/17
 * server.c - a chat server (and monitor) that uses pipes and sockets
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>


#define MAX_CLIENTS 10

// constants for pipe FDs
#define WFD 1
#define RFD 0


/*
 * nonblock - a function that makes a file descriptor non-blocking
 * @param fd file descriptor
 */
void nonblock(int fd) {
  int flags;

  if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
    perror("fcntl (get):");
    exit(1);
  }
  if (fcntl(fd, F_SETFL, flags | FNDELAY) == -1) {
    perror("fcntl (set):");
    exit(1);
  }
}


/*
 * monitor - provides a local chat window
 * @param srfd - server read file descriptor
 * @param swfd - server write file descriptor
 */
void monitor(int srfd, int swfd) {
  ssize_t byt;
  char buf[1025];
  int err;
  fd_set fdset;

  while (1) {
    //preparing fdset
    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset);
    FD_SET(srfd, &fdset);
    
    err = select(srfd + 1, &fdset, NULL, NULL, NULL);
    
    if (err < 0) {
      perror("select mon");
      exit(10);
    } else {
      //checking if there is anything to print from the server
      if (FD_ISSET(srfd, &fdset)) { 
        byt = read(srfd, buf, sizeof(buf));
        if (byt < 0) {
          perror("read");
          exit(11);
        } else if (byt == 0) {
          break;
        }
        write(STDOUT_FILENO, "msg: ", 6);
        write(STDOUT_FILENO, buf, byt);
      }
      //checking if there is anything to communicate to the server
      if (FD_ISSET(STDIN_FILENO, &fdset)) {
        byt = read(STDIN_FILENO, buf, sizeof(buf));
        if (byt < 0) {
          perror("read");
          exit(11);
        } else if (byt == 0) { 
          break;
        }
        write(swfd, buf, byt);
      }
    }
  }
}




/*
 * server - relays chat messages
 * @param mrfd - monitor read file descriptor
 * @param mwfd - monitor write file descriptor
 * @param portno - TCP port number to use for client connections
 * @param host - name of host server
 * @param ip - an int to decide if connect/disconnect messages should print
 */
void server(int mrfd, int mwfd, int portno, char *host, int ip) {
  int SIZE = 10;
  char **ipa = malloc(2048);

  struct sockaddr_in mine;
  socklen_t slen;
  struct hostent *he;
  int nfd, sfd, err, byt, ct, max;
  char buf[1025];
  int cfd[SIZE];
  fd_set fdset;
  struct timeval timeout;

  memset(cfd, 0, sizeof(cfd));
  memset(&fdset, 0, sizeof(fdset));
  memset(&timeout, 0, sizeof(timeout));

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd < 0) {
    perror("socket");
    exit(6);
  }
  nonblock(sfd);

  //creates a socket address object to connect to port
  he = gethostbyname(host);
  portno = htons(portno);
  mine.sin_family = AF_INET;
  mine.sin_port = portno;
  memcpy(&mine.sin_addr.s_addr, he->h_addr_list[0], he->h_length);

  err = bind(sfd, (struct sockaddr *)&mine, sizeof(mine));  
  if (err < 0) {
    perror("bind");
    exit(7);
  }

  err = listen(sfd, 16);
  if (err < 0) {
    perror("listen");
    exit(8);
  }
  
  //count is 1 to account for the monitor read file being cfd[0]
  //timeout is set to 0.1s
  timeout.tv_sec = 0;
  timeout.tv_usec = 100000;
  cfd[0] = mrfd;
  ct = 1;
  
  do {
    //checking if the monitor has sent an EOF and mrfd has closed
    if (cfd[0] == 0) {
      break;
    }
    
    //prep the fdset
    FD_ZERO(&fdset);    

    nfd = accept(sfd, (struct sockaddr *)&mine, &slen);
    if (nfd < 0) {
      if (errno == EWOULDBLOCK) { //disregards nonblocking errors
      } else {
        perror("accept"); //handles bad errors
        exit(8);
      } 
    } else {
      nonblock(nfd);
      if (cfd[ct] == 0) {
        cfd[ct] = nfd; //adds new fd to fd list if the next spot in the array is open
        if (ip == 1) {
          ipa[ct] = inet_ntoa(mine.sin_addr);
          printf("client has connected from %s\n", inet_ntoa(mine.sin_addr)); fflush(stdout);
        }
        ct++; 
        ct %= SIZE; //increments ct and keeps it within valid array indices
      } else {
        int k; //initialized outside the loop so it can be used to check if ct was set and the nfd was added
        for (k = 1; k < SIZE; k++) { //k is initialized to 1 because the mrfd will never be 0 if the program is running
          if (cfd[k] == 0) {
            cfd[k] = nfd;
            ct = k; //only executed if there is space in the array and the client is added
            if (ip == 1) {
              ipa[ct] = inet_ntoa(mine.sin_addr);
              printf("client has connected from %s\n", inet_ntoa(mine.sin_addr)); fflush(stdout);
            }
            break;
          }
        }
        if (k != ct) {
          write(nfd, "Too many clients, try again later.", 35); fflush(stdout);
          close(nfd);
        }
      }
    }
    //initializes the fdset for select and finds the max fd
    max = cfd[0];
    for (int i = 0; i < SIZE; i++) {
      if (cfd[i] > 0) {
        FD_SET(cfd[i], &fdset);
        if (cfd[i] > max) { 
          max = cfd[i]; 
        }
      }
    }

    err = select(max + 1, &fdset, NULL, NULL, &timeout); //looks for "interesting things" in fdset
    if (err < 0) {
      perror("select");
      exit(9);
    }
    
    //iterates through for loop to check if the fd is set by select
    for (int i = 0; i < SIZE; i++) {
      if (FD_ISSET(cfd[i], &fdset)) {
        //reads from the set fd
        byt = read(cfd[i], buf, sizeof(buf));
        if (byt > 0) {
          //write the message to the monitor if it wasn't from the monitor
          if (cfd[i] != mrfd) {
            write(mwfd, buf, byt);
          }
        } else if (byt == 0) {
          //closing fds that return an EOF
          close(cfd[i]);
          cfd[i] = 0;
          if (ip == 1 && i != 0) {
            printf("client at address %s has disconnected\n", ipa[i]); fflush(stdout);
          }
        } else {
          perror("read");
          exit(13);
        } 

        //writes the message to all clients
        for (int j = 0; j < SIZE; j++) {
          if (j != i && cfd[j] != 0) {
            write(cfd[j], buf, byt);
          }
        }
      }
    }
  } while (1);
  
  //closes all remaining client file descriptors when server disconnects first
  for (int i = 0; i < SIZE; i++) {
    if (cfd[i] > 0) {
      printf("client at address %s has been disconnected by the server\n", ipa[i]); fflush(stdout);
      close(cfd[i]);
    }
  }
  free(ipa);
  printf("hanging up now\n");
  fflush(stdout);
}




int main(int argc, char **argv) {
  int IP = 0;
  int pnum = 50497;
  char *hostname = "localhost";
  
  pid_t p;
  int err, opt, status;
  int stm[2], mts[2];

  while ((opt = getopt(argc, argv, "h:p:c")) >= 0) {
    switch (opt) {
      //for setting the host
      case 'h':
        hostname = strdup(optarg);
        break;

      //for setting the port number
      case 'p':
        pnum = atoi(optarg);
        break;

      //for printing connect/disconnect messages
      case 'c':
        IP = 1;
        break;

      //for misused flags
      default:
        printf("usage: ./client [-h name] [-p prt #]\n");
        printf("\t -h: host name to connect to\n");
        printf("\t -p #: the port to use when connecting to the server\n");
        printf("\t -c: tells server to print IP address connections\n");
        fflush(stdout);
        break;
    }
  }

  //establishing a pipe for the monitor
  err = pipe(stm);
  if (err < 0) {
    perror("monitor pipe");
    exit(2);
  }

  //establishing a pipe for the relay server
  err = pipe(mts);
  if (err < 0) {
    perror("relay server pipe");
    exit(3);
  }

  //forking for child (monitor) process and parent (relay server) process
  p = fork();
  if (p < 0) {
    perror("fork");
    exit(1);
  }
  else if (p == 0) {
    //child process
    //closes extra pipe fd from fork()
    close(stm[WFD]);
    close(mts[RFD]);
    nonblock(stm[RFD]);
    nonblock(mts[WFD]);
    monitor(stm[RFD], mts[WFD]);
    close(stm[RFD]);
    close(mts[WFD]);
    exit(4);
  }

  else {
    //parent process
    //closes extra pipe fd from fork()
    close(mts[WFD]);
    close(stm[RFD]);
    nonblock(mts[RFD]);
    nonblock(stm[WFD]);
    server(mts[RFD], stm[WFD], pnum, hostname, IP);
    close(mts[RFD]);
    close(stm[WFD]);
    wait(&status);
  }

  free(hostname);

  return 0;
}
