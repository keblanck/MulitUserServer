/* 
 * Kathleen Blanck
 * 10/20/17
 * client.c - a program to connect to a chat server
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>

void nonblock(int fd) {
  int flags;

  if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
    perror("fcntl (get)");
    exit(1);
  }
  if (fcntl(fd, F_SETFL, flags | FNDELAY) == -1) {
    perror("fcntl (set)");
    exit(2);
  }
}

int main(int argc, char **argv) {
  int pnum = 50497;
  char *hostname = "localhost";
  
  struct sockaddr_in toServ;
  socklen_t slen;
  int csfd, opt, err, byt;
  char buf[1025];
  struct hostent *he;
  fd_set fdset;

  while ((opt = getopt(argc, argv, "h:p:c")) >= 0) {
    switch (opt) {
      //for setting the port number
      case 'p':
        pnum = atoi(optarg);
        break;

      //for setting the server host
      case 'h':
        hostname = strdup(optarg);
        break;

      //realistically doesn't matter because client doesn't do anything with this and we don't have to worry about non-matching tags according to the assignment. would take more thought and process communication if we had to
      case 'c':
        break;

      //for misuse of flags
      default:
        printf("usage: ./client [-h] [-p prt #]\n");
        printf("\t -h: help message\n");
        printf("\t -p #: the port to use when connecting to the server\n");
        printf("\t -c: tells the server to print IP address of connections\n");
        fflush(stdout);
        break;
    }
  }


  csfd = socket(AF_INET, SOCK_STREAM, 0);
  if (csfd < 0) {
    perror("client socket");
    exit(1);
  }
  nonblock(csfd);

  //creates a socket address object made to connect to server
  he = gethostbyname(hostname);
  pnum = htons(pnum);
  toServ.sin_family = AF_INET;
  toServ.sin_port = pnum;
  memcpy(&toServ.sin_addr.s_addr, he->h_addr_list[0], he->h_length);


  err = connect(csfd, (struct sockaddr *)&toServ, sizeof(toServ));
  if (err < 0) {
    if (errno == EINPROGRESS) {
    } else {
      perror("connect");
      exit(2);
    }
  }
  printf("connected to server\n\n");
  fflush(stdout);

  while (1) {
    //preps fdset
    FD_CLR(STDIN_FILENO, &fdset);
    FD_CLR(csfd, &fdset);
    FD_SET(STDIN_FILENO, &fdset);
    FD_SET(csfd, &fdset);
    
    err = select(csfd + 1, &fdset, NULL, NULL, NULL);
    
    if (err < 0) {
      perror("select client");
      exit(10);
    } else {
      if (FD_ISSET(csfd, &fdset)) {
        //reads from connection to server for anything to print to the screen
        byt = read(csfd, buf, sizeof(buf));
        if (byt < 0) {
          perror("read");
          exit(11);
        } else if (byt == 0) {
          break;
        }
        write(STDOUT_FILENO, "msg: ", 6);
        write(STDOUT_FILENO, buf, byt);
      } 
      if (FD_ISSET(STDIN_FILENO, &fdset)) {
        //reads from the keyboard and sends it to server
        byt = read(STDIN_FILENO, buf, sizeof(buf));
        if (byt < 0) {
          perror("read");
          exit(12);
        } else if (byt == 0) {
          write(csfd, buf, byt);
          break;
        }
        write(csfd, buf, byt);
      }
    }
  }

  //clean clean clean
  close(csfd);
  free(hostname);
  printf("hanging up now\n");
  fflush(stdout);
  return 0;

}


