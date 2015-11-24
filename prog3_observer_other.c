/* prog1_client.c - Modified from demo_client.c created by Brian Hutchinson.
 Lines 90+ written specifically for this assignment
 Cat Felts and Dannon Dixon
 CS 367, Assignment 1 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>



/*------------------------------------------------------------------------
 * Program: client
 *
 * Purpose: allocate a socket, connect to a server, and print all output
 *
 * Syntax: client [ host [port] ]
 *
 * host - name of a computer on which server is executing
 * port - protocol port number server is using
 *
 * Note: Both arguments are optional. If no host name is specified,
 * the client uses "localhost"; if no protocol port is
 * specified, the client uses the default given by PROTOPORT.
 *
 *------------------------------------------------------------------------
 */
main( int argc, char **argv) {
  struct hostent *ptrh; /* pointer to a host table entry */
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold an IP address */
  int sd; /* socket descriptor */
  int port; /* protocol port number */
  char *host; /* pointer to host name */
  int n; /* number of characters read */
  char buf[1000]; /* buffer for data from the server */

  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET; /* set family to Internet */

  if( argc != 3 ) {
    fprintf(stderr,"Error: Wrong number of arguments\n");
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"./client server_address server_port\n");
    exit(EXIT_FAILURE);
  }

  port = atoi(argv[2]); /* convert to binary */
  if (port > 0) /* test for legal value */
    sad.sin_port = htons((u_short)port);
  else {
    fprintf(stderr,"Error: bad port number %s\n",argv[2]);
    exit(EXIT_FAILURE);
  }

  host = argv[1]; /* if host argument specified */

  /* Convert host name to equivalent IP address and copy to sad. */
  ptrh = gethostbyname(host);
  if ( ptrh == NULL ) {
    fprintf(stderr,"Error: Invalid host: %s\n", host);
    exit(EXIT_FAILURE);
  }

  memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

  /* Map TCP transport protocol name to protocol number. */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }

  /* Create a socket. */
  sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  /* Connect the socket to the specified server. */
  if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
    fprintf(stderr,"connect failed\n");
    exit(EXIT_FAILURE);
  }

  /* Repeatedly read data from socket and write to user's screen. */
  int m = -1;
  uint32_t guess = -1;
  char response = -1;
  while(response != '0'){
    fprintf(stderr, "Enter guess: \n");
    scanf("%d",&guess);
    guess = htonl(guess);
    send(sd,&guess,4,0);
    m = recv(sd, &response, 2, 0);
    if(response == '1')
      fprintf(stderr, "Warmer\n");
    else if(response == '2')
      fprintf(stderr, "Colder\n");
    else if(response == '3')
      fprintf(stderr, "Same\n");
  }
  fprintf(stderr, "You Win!\n");
  close(sd);
  exit(EXIT_SUCCESS);
}
