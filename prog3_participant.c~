/*Ben Thompson, Cat Feltz
 *Project 2: Sudoku
 *CS367 Fall 2015
 *10/26/15
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

	if(recv(sd, &player_number, sizeof(player_number), 0) <=0){
		close(sd);
		exit(1);
	}
	printf("Player %c\n", player_number);
	
	//game loop
	while (1) {

//CLIENT RECV
//===================================================================================

	//***RECV turn***
		if(recv(sd, &player_turn, sizeof(player_turn), 0) <= 0){

			break;
		}

		//end game logic
		if(player_turn == 'Z'){
			if(player1_score > player2_score){
				printf("Player 1 wins! ");

			}else if(player1_score < player2_score){
				printf("Player 2 wins! ");
			}else{
				printf("Draw. ");
			}
			printf("Game over!\n");
			break;
		}

	//***RECV recent move:***
		if(recv(sd, &prev_move, sizeof(prev_move), 0) <= 0){
			break;
		}

	//***RECV score:***
		//recv player1 score
		if(recv(sd, &temp_score, sizeof(temp_score), 0) <= 0){
			break;
		}
		player1_score = ntohl(temp_score);

		//recv player 2 score
		if(recv(sd, &temp_score, sizeof(temp_score), 0) <= 0){
			break;
		}
		player2_score = ntohl(temp_score);

	//***RECV board***
		if(recv(sd, &game_board, sizeof(game_board), 0) <= 0){
			break;
		}
//============================================================================================

		//player turn logic
		if(player_turn == 'N'){ //your turn
			if(prev_move == 'P'){
				printf("You passed\n");
			}else if(prev_move == '1'){
				printf("Your move was valid! +1\n");
			}else if(prev_move == 'X'){
				printf("Your move was invalid! -1\n");
			}else if(prev_move == '0'){
				printf("Let the games begin!\n");
			}else{
				printf("error receiving prev move.\n");
			}
		}else{//opponents turn
			if(prev_move == 'P'){
				printf("The enemy passed.\n");
			}else if(prev_move == '1'){
				printf("The enemy's move was valid! >:(\n");
			}else if(prev_move == 'X'){
				printf("The enemy's move was invalid! :D\n");
			}else if(prev_move == '0'){
				printf("Let the games begin!\n");
			}else{
				printf("error receiving prev move\n");
			}
		}

		printBoard(game_board);
		printf("Player 1 score: %d \nPlayer 2 score: %d \n", player1_score, player2_score);

		//SEND player move
		if(player_turn == 'Y'){
			printf("Your turn: ");
			scanf("%s", input);
			if(send(sd, &input, sizeof(input), 0) <= 0){
				break;
			}
		}else if(player_turn == 'N'){
			printf("Waiting turn...\n");
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