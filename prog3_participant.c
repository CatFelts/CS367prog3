/*
*This program was extended from prog2_client.c written by
*Ben Thompson and Cat Felts for CS367 Fall 2015 on 10/26/15
*
*Cat Felts
*CS367 Fall 2015
*11/15/15
*
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <limits.h>

#define DEBUG 1
#define MAX_ANSWER 3 //Max answer should be no more than 3 eg. A32 or P for pass
#define MAX_LINE 1000
#define BOARD_SIZE 9

void printBoard(char gboard[][BOARD_SIZE]);
/*------------------------------------------------------------------------
* Program: Network Sudoku
*
* Purpose: Server hosts connection with 2 clients and moderates game of competitive sudoku.
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
int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	char *host; /* pointer to host name */
	int n = 1; /* number of characters read */
	char player_number;
	uint32_t player1_score;
	uint32_t player2_score;
	uint32_t temp_score;
	char input[MAX_ANSWER];
	char game_board[BOARD_SIZE][BOARD_SIZE];
	char player_turn;
	char prev_move;

	//=============connect to server==============//

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

	//=============/!connect to server!/==============//
	char username[62];
	char username_valid = 'N';
	#if DEBUG
	printf("Participant %d connected to server! Hooray!\n", sd);
	#endif
	while(username_valid != 'Y'){

		printf("Enter a username:\n");
		scanf("%s", username);
		if(send(sd, &username, sizeof(username), 0) <= 0){
			fprintf(stderr, "Error: Send username to server failed.\n");
			exit(EXIT_FAILURE);
		}

		#if DEBUG
		printf("username %s sent to server.", username);
		#endif
		//recv validity of username from server
		if(recv(sd, &username_valid, sizeof(username_valid), 0)<= 0){
			fprintf(stderr, "Error: recv username validity from server.\n");
			exit(EXIT_FAILURE);
		}
		#if DEBUG
		printf("username validity = %c\n", username_valid);
		#endif
	}

	//game loop
	while (1) {

		#if DEBUG
		printf("participant entered game loop! (: \n");
		#endif

		//make a move
		printf("Enter a valid move: \n");
		scanf("%s", input);
		#if DEBUG
		printf("user entered the following move --> %s\n", input);
		printf("size of input = %lu\n", sizeof(input));
		#endif

		//send that move to the server
		if(send(sd, &input, sizeof(input), 0) <= 0){
			fprintf(stderr, "Error: could not send players move to server.\n");
			exit(EXIT_FAILURE);
		}
	}


	close(sd);
	return 0;
}


//prints gameboard
void printBoard(char gboard[][BOARD_SIZE]){
	int i,j;
	int count;
	char row = 'A';
	char col;

	printf("  | 1 2 3 | 4 5 6 | 7 8 9 |");
	for(i = 0; i < BOARD_SIZE; i++){
		puts("");
		if((i % 3) == 0){
			printf("--+-------+-------+-------+\n");
		}
		printf("%c", row);
		row++;
		for(j = 0; j < BOARD_SIZE; j++){
			if((j % 3) == 0){
				printf(" |");
			}
			if(gboard[i][j] == '0'){
				printf("  ");
			}else{
				printf(" %c", gboard[i][j]);
			}
		}
		printf(" |");
	}
	printf("\n--+-------+-------+-------+\n");
}
