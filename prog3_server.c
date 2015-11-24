/*
*This program is an extension of prog2_server.c written by
*Ben Thompson and Cat Felts for Program 2 in CS367 Fall 2015
*on 10/26/15
*
*
*
*Cat Felts
*Program 3
*CS367 Fall 2015
*11/15/15
*/

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#define QLEN 6 /* size of request queue */
#define DEBUG 1
#define MAX_ANSWER 1000 //max for 9x9 gameboard
#define BOARD_SIZE 9
#define MAX_VALID_MOVES BOARD_SIZE * BOARD_SIZE

enum{
	ROW,
	COLUMN,
	VALUE,
};



void init_board(char board[][BOARD_SIZE]);
char *setup_board_message(char board[][BOARD_SIZE]);
void printboard(char board[][BOARD_SIZE]);
int moveIsValid(char move[], char board[][BOARD_SIZE]);
int usernameIsValid(const char* username, char * current_usernames[1024]);
int blockIsValid(int row, int col, char val, char board[][BOARD_SIZE]);
/*------------------------------------------------------------------------
* Program: Network Sudoku
*
* Purpose: Server hosts connection with 2 clients and moderates game of competitive sudoku.

* Syntax: server [ port ]
*
* port - protocol port number to use
*
* Note: The port argument is optional. If no port is specified,

* the server uses the default given by PROTOPORT.
*
* Network Byte Order - htons(unsigned short hostshort) //host short to network short
*
*
*------------------------------------------------------------------------
*/
int main(int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad1, sad2; /* structures to hold server's address */
	struct sockaddr_in cad; /* structure to hold client's address */
	int observ_sd, partic_sd, sd0, sd1, sd2; /* socket descriptors */
	//observ_sd and partic_sd for initial setup
	int port; /* protocol port number */
	int observ_port; /*protocol port number where the server will listen for observers*/
	int partic_port; /*protocol port number where the server will listen for participants*/
	int alen; /* length of address */
	int optval = 1; /* boolean value when we set socket option */
	int max_observers; /* the maximum number of observers as specified by command line args */
	int max_participants; /* the maximum number of participants as specified by command line args */
	int N; /* length of each round in second as specified by command line args */
	int K; /* length of the pause between each round in seconds as specified by command line args */
	char player_number;
	char partic_number;
	char y_player_turn = 'Y';
	char n_player_turn = 'N';
	char prev_move = '0';
	char observer_reply[MAX_ANSWER];
	char game_board[BOARD_SIZE][BOARD_SIZE];
	unsigned pass_count;
	unsigned valid_count;
	uint32_t temp_score;
	char game_running;
	char first_turn;
	char player_move[MAX_ANSWER];
	char *msg = "This is the message that will contain the game info.\n.";

	//=================Open Connection===================//

	//check args
	if( argc !=  7) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server server_port\n");
		exit(EXIT_FAILURE);
	}
	/* clear sockaddr structures */
	memset((char *)&sad1,0,sizeof(sad1));
	memset((char *)&sad2, 0, sizeof(sad2));
	/* set family to Internet */
	sad1.sin_family = AF_INET;
	sad2.sin_family = AF_INET;
	/* set the local IP address */
	sad1.sin_addr.s_addr = INADDR_ANY;
	sad2.sin_addr.s_addr = INADDR_ANY;
	/*convert 1st argument (port for observers) to binary*/
	observ_port = atoi(argv[1]);
	/*convert 2nd argument (port for participants) to binary*/
	partic_port = atoi(argv[2]);

	/* set the maximum number of observers at any one time during the game */
	max_observers = atoi(argv[3]);

	/* set the maximum number of participants at any one time during the game */
	max_participants = atoi(argv[4]);

	/* set round length */
	N = atoi(argv[5]);
	if(N <= 0){
		fprintf(stderr, "Error: Round length must be positive\n");
		exit(EXIT_FAILURE);
	}

	/* set inter-round pause */
	K = atoi(argv[6]);
	if(K <= 0){
		fprintf(stderr, "Error: Inter-round pause length must be positive\n");
		exit(EXIT_FAILURE);
	}

	/*test for illegal value for the observer port*/
	if(observ_port > 0){
		sad1.sin_port = htons((u_short)observ_port);
	}else{
		fprintf(stderr, "Error: Bad observer port number %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	/*test for illegal value for the participant port*/
	if(partic_port > 0){
		sad2.sin_port = htons((u_short)partic_port);
	}else{
		fprintf(stderr, "Error: Bad participant port number %s\n", argv[2]);
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket for the observers */
	observ_sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if(observ_sd < 0){
		fprintf(stderr, "Error: Oberserver socket creation failed\n");
		exit(EXIT_FAILURE);
	}


	/* Create a socket for the participants */
	partic_sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if(partic_sd < 0){
		fprintf(stderr, "Error: Participant socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of ports - avoid "Bind failed" issues */
	if(setsockopt(observ_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0){
		fprintf(stderr, "Error Setting observer socket option failed\n");
		exit(EXIT_FAILURE);
	}
	if(setsockopt(partic_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0){
		fprintf(stderr, "Error Setting participant socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind local addresses to the sockets */
	if (bind(observ_sd, (struct sockaddr *)&sad1, sizeof(sad1)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}
	if (bind(partic_sd, (struct sockaddr *)&sad2, sizeof(sad2)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queues */
	if (listen(observ_sd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen for observer failed\n");
		exit(EXIT_FAILURE);
	}
	if (listen(partic_sd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen for participant failed\n");
		exit(EXIT_FAILURE);
	}

	printf("Server running. Listening for clients...\n");

	//================/!Open Connection!/=================//
	double n = 0;       /* how much time has elapsed since round has begun */
	double k = 0;       /* how much time has elapsed since the last round ended */
	char y_reply = 'Y';
	char n_reply = 'N';
	char *partic_usernames[1024]; /* keep an array of the participants usernames  -- NEED TO MAKE [][]*/
	uint32_t partic_scores[max_participants];    /* keep an array of the participants scores */
	int move_valid = -1;         /* store the validity of checking a participants move */
	int i = 0;
	int activity, valread = 0;
	int num_observers = 0;       /* number of observers currently connected */
	int num_participants = 0;       /* number of participants currently connected */
	int sd = 0;        /* place holder to keep track of current socket descriptor */
	int max_sd =0 ;
	clock_t start, end;     /* a reference to the time at the start and end of a round */
	int observer_sds[max_observers]; /* a reference to all the observer socket descriptors */
	int participant_sds[max_participants]; /*a reference to all the participant socket descriptors */
	fd_set read_fds;
	FD_ZERO(&read_fds);
	struct timeval stimeout = {N, 0};

	//fill observers_sd and particpatns_sds with -1 to indicate no observers/participants currently connected */
	for(i = 0; i < max_observers; i++){
		observer_sds[i] = -1;
	}
	for(i = 0; i< max_participants; i++){
		participant_sds[i] = -1;
		//also make all scores = 0
		partic_scores[i] = 0;
		partic_usernames[i] = '\0';
	}




	/* ------------------------------------ Q's -------------------------------------------
	*
	* 1. How to build strings, return them from a function, assign to a variable, etc
	*
	*/

	//initial starting conditions
	pass_count = 0;
	init_board(game_board);
	char *board_msg = setup_board_message(game_board);
	char * move_msg = (char *)malloc(70 * sizeof(char) +2);

	//char *board_msg= "nothing here yet.";
	//char *board_msg[8];
	//setup_board_message(board_msg, game_board);
	//const char* gameboard_msg = setup_board_message(&game_board);
	#if DEBUG
	printf("%s\n",board_msg);
	#endif




	/* Main server loop - accept and handle requests
	/* game loop */
	while (1) {
		#if DEBUG
		printf("list of current usernames = \n");
		for(i = 0; i<max_participants; i++){
			printf("[%d] = %s\n", i, partic_usernames[i]);
		}
		#endif

		start = clock();
		n = (double)(end - start)/CLOCKS_PER_SEC;
		if(n < N){
			alen = sizeof(cad); //client addr

			#if DEBUG
			printf("Waiting for connections...\n");
			#endif

			max_sd = 0;

			//clear out fd_sets for observers and participants
			FD_ZERO(&read_fds);

			//add master sockets to read set
			FD_SET(observ_sd, &read_fds);
			FD_SET(partic_sd, &read_fds);

			max_sd = observ_sd > partic_sd? observ_sd : partic_sd ;

			//add all the participant and observer sockets
			for(i = 0; i<max_participants; i++){
				sd = participant_sds[i];
				if( sd > 0){
					FD_SET(participant_sds[i], &read_fds);
					max_sd = sd > max_sd ? sd : max_sd ;
				}
			}
			for(i = 0; i<max_observers; i++){
				sd = observer_sds[i];
				if(sd >0){
					FD_SET(observer_sds[i], &read_fds);
					max_sd = sd > max_sd ? sd : max_sd ;
				}
			}



			//wait for an activity on one of the sockets, timeout == N
			#if DEBUG
			printf("Server has gotten to the select() functions.\n");
			#endif


			activity = select(max_sd +1, &read_fds, NULL, NULL, &stimeout);
			#if DEBUG
			printf("server has gotten past the select() function.\n");
			#endif

			if(activity < 0){
				fprintf(stderr, "ERROR: select failed.\n");
				exit(EXIT_FAILURE);
			}
			else if(activity == 0){
				fprintf(stderr, "select() timeout period expired.\n");
			}

			//if something trying to read on the observer master socket, its an incoming connection
			if(FD_ISSET( observ_sd, &read_fds)){
				if((sd1 = accept(observ_sd, (struct sockaddr *)&cad, &alen)) < 0){
					fprintf(stderr, "ERROR: Accept observer failed.\n");
					exit(EXIT_FAILURE);
				}

				//if there are already the max amount of observers, immediately close the connection
				if(num_observers == max_observers)
				close(sd1);
				else{
					num_observers++;

					#if DEBUG
					printf("New observer connnected, socket fd is %d\n", sd1);
					#endif

					//add new socket to array of sockets
					for(i = 0; i<max_observers; i++){
						//if position is empty
						if(observer_sds[i] <= 0)
						{
							observer_sds[i] = sd1;
							#if DEBUG
							printf("adding to list of observer sockets as %d\n", i);
							#endif

							break;
						}
					}
				}

			}


			//else if something happened its an observer trying to close the connection
			for(i = 0; i<max_observers; i++){
				sd = observer_sds[i];

				#if DEBUG
				printf("Check if observer %d is trying to disconnect.\n", sd);
				#endif

				//if the participant on this socket wants to give the server something
				if(FD_ISSET(sd, &read_fds)){
					//check to see if its trying to close the connection
					if(recv(sd, &observer_reply, sizeof(observer_reply), 0) == 0){
						//some observer has disconnected
						#if DEBUG
						printf("host observer disconnected %d\n", sd);
						#endif

						close(sd);
						observer_sds[i] = -1;\
						num_observers --;
					}


				}
			}


			//if something happened on the participant master socket, its an incoming connection
			if(FD_ISSET(partic_sd, &read_fds)){
				if((sd2 = accept(partic_sd, (struct sockaddr *)&cad, &alen)) < 0){
					fprintf(stderr, "ERROR: Accept participant failed.\n");
					exit(EXIT_FAILURE);
				}

				//if there is already the max number of participants, then immediately close the connection
				if(num_participants == max_participants)
				close(sd2);
				else{
					num_participants++;
					#if DEBUG
					printf("New participant connected, socket fd is %d\n", sd2);
					#endif

					// ------------------------- get username and send validity---------
					int username_valid = -1;
					char username[64];


					while(username_valid != 1){
						//get participants username
						if(recv(sd2, username, sizeof(username), 0) <= 0){
							fprintf(stderr, "Error: Could not recv username from participant.\n");
							exit(EXIT_FAILURE);
						}

						//check for validity of username
						username_valid = usernameIsValid(username, partic_usernames);
						#if DEBUG
						printf("is username valid? --> %d\n", username_valid);
						#endif

						//send validity of username
						if(username_valid == 1){
							//send 'Y" to participant
							#if DEBUG
							printf("sending 'Y' to participant.\n");
							#endif

							if(send(sd2, &y_reply, sizeof(y_reply), 0) <= 0){
								fprintf(stderr, "Error: reply y to participant's username.\n");
								exit(EXIT_FAILURE);
							}


						}else{
							//send 'N' to participant
							#if DEBUG
							printf("sending 'N' to participant.\n");
							#endif
							if(send(sd2, &n_reply, sizeof(n_reply), 0)<= 0){
								fprintf(stderr, "Error: reply n to participant's username.\n");
								exit(EXIT_FAILURE);
							}
						}
					}

					#if DEBUG
					printf("participant has picked a valid username, \nAdd to array!\n");
					#endif

					//add new socket to array of sockets
					for(i = 0; i<max_participants; i++){
						//if position is empty
						if(participant_sds[i] == -1){
							participant_sds[i] = sd2;
							//add the username to the list of partic_usernames
							//sprintf(&partic_usernames[i], "%s", username);
							partic_usernames[i] = username;
							#if DEBUG
							printf("adding to list of participant sockets as %d\n", i);
							printf("added %s to list of usernames.\n", username);
							#endif

							break;
						}
					}
				}
			}

			//else if something happened its an incoming move from a participant or the participant trying to close the connection
			for(i = 0; i<max_participants; i++){
				sd = participant_sds[i];

				//if the participant on this socket wants to give the server something
				if(FD_ISSET(sd, &read_fds)){
					//check to see if its trying to close the connection and also read what its trying to send
					if(valread = recv(sd, &player_move, 3, 0) == 0){
						//some participant has disconnected
						#if DEBUG
						printf("host participant disconnected %d\n", sd);
						#endif

						close(sd);
						participant_sds[i] = -1;
						num_participants --;
					}

					else{
						//
						strcat(player_move, "");
						#if DEBUG
						printf("this is what was read from the participant: %s\n", player_move);
						printf("clear out move_msg before writing to it\n");
						#endif
						move_msg = '\0';
						#if DEBUG
						printf("after clearing out move_msg -->  %s\n", move_msg);
						#endif
						move_msg = partic_usernames[i];

						//check to see if valid move
						if(move_valid = moveIsValid(player_move, game_board) == 1){
							//update gameboard accordingly
							game_board[(player_move[ROW]-64)-1][(player_move[COLUMN]-48)-1] = player_move[VALUE];

							
							strcat(move_msg, " made valid move ");
							strcat(move_msg, player_move);
							strcat(move_msg, "\n");
							#if DEBUG
							printf("current move_msg sending to observers-->  %s\n", move_msg);
							#endif
						}else{
							//make invalid move message
							strcat(move_msg, " attempted to make an invalid move.\n");
						}

						//send validity of move and game_board to all observers
						board_msg = setup_board_message(game_board);
						//send validity to observers
						for(i = 0; i<max_observers; i++){
							sd = observer_sds[i];
							#if DEBUG
							printf("sending move msg and game board msg to observer %d\n", sd);
							#endif
							
							//if this is a valid connected observer
							if(sd >0){
								//send move_msg
								if(send(sd, &move_msg, sizeof(move_msg), 0) <= 0){
									fprintf((stderr), "Error: sending move_msg to observer %d\n", sd);
									exit(EXIT_FAILURE);
								}
								//then send the board
								if(send(sd, &board_msg, sizeof(board_msg), 0) <= 0){
									fprintf((stderr), "Error: sending board_msg to observer %d\n", sd);
									exit(EXIT_FAILURE);
								}

							}

						}
					}
				}

				
			}

			end = clock();
			#if DEBUG
			printf("end of cycle.\n");
			#endif

		}
		//see if its the end of the round
		else{
			#if DEBUG
			printf("end of round.\n");
			#endif

			//send leaderboard to observers

			//reset round clock
			n = 0;

			//wait for K seconds before starting another round
			while(k < K){
				start = clock();
				end = clock();
				k = (double)(end - start)/CLOCKS_PER_SEC;
			}
		}
		//game loop end
		//send leaderboard to all observers
	}

	//main end
}

/*Input: size of board, board pointer
*Ouput: none
*Initialize all board elements to '0'
*/
void init_board(char board[][BOARD_SIZE]){
	int i,j;
	char row = 'A';
	char col;


	for(i = 0; i < BOARD_SIZE; i++){
		for(j = 0; j < BOARD_SIZE; j++){
			board[i][j] = '0';
		}
	}
}




/* creates a formatted message containing the board to send to the observer */
char * setup_board_message(char board[][BOARD_SIZE]){
	char * msg = (char *)malloc(28 * 10 * sizeof(char) + 1 + 1);
	msg[0] = '\0';
	int i, j;
	int count;
	char row = 'A';
	char col;
	char c_to_str[2];
	c_to_str[1] = '\0';
	c_to_str[0] = row;
	char * str1 = "  | 1 2 3 | 4 5 6 | 7 8 9 |";
	strcat(msg, str1);

	for(i = 0; i < BOARD_SIZE; i++){
		strcat(msg, "\n");

		if((i%3) == 0){
			strcat(msg,"--+-------+-------+-------+\n");
		}
		c_to_str[0] = row;
		strcat(msg, c_to_str);
		row++;

		for(j = 0; j < BOARD_SIZE; j++){
			if((j % 3) == 0){
				strcat(msg, " |");
			}
			if(board[i][j] == '0'){
				strcat(msg, "  ");
			}else{
				c_to_str[0] = board[i][j];
				strcat(msg, c_to_str);
			}
		}
		strcat(msg, " |");
	}
	strcat(msg, "\n--+-------+-------+-------+\n");

	return msg;
}






/*Input: player's char move[], game_board[][].
*Output: 1 if valid, 0 if invalid
*
*Checks to see if the player move is valid per sudoku rules.
*	-Valid in grid: column 1-9 | row A-I.
*	-Valid input: no duplicate values (1-9) on row, column or 3x3 block.
*  -Otherwise move is invalid.
*/
int moveIsValid(char move[], char board[][BOARD_SIZE]){
	int row;
	int col;
	char val;
	int i,j;

	//parse player move;
	row = (move[ROW] - 64)-1; //ascii conversion
	col = (move[COLUMN] - 48)-1;
	val = move[VALUE];

	#if DEBUG
	printf("CHAR MOVE: row:%c, col:%c, val:%c\n", move[0], move[1], move[2]);
	printf("\nplayer move: Row: %d, Col: %d, Value: %c\n", row, col, val);
	printf("Element at %d, %d = '%c'\n", row, col, board[row][col]);
	#endif

	//check if element is available
	if(board[row][col] == '0'){

		//check row
		for(i = 0; i < BOARD_SIZE; i++){
			if(board[row][i] == val){
				return 0;
			}
		}
		//check col
		for(i = 0; i < BOARD_SIZE; i++){
			if(board[i][col] == val){
				return 0;
			}
		}
		//check block
		if(!blockIsValid(row, col, val, board)){
			return 0;
		}
		#if DEBUG
		printf("Move is valid.\n");
		#endif
		return 1;
	}else{
		//invalid move
		#if DEBUG
		printf("invalid move");
		#endif
		return 0;
	}
}


int usernameIsValid(const char* username, char * current_usernames[1024]){
	int username_index = 0;
	char l = username[username_index];
	char *name = (char *)malloc(30 * sizeof(char) +2);
	int i;

	// -----------------------------NEED TO CMP TO PREV USERNAMES ------------------
	/*
	for(i = 0; i<sizeof(current_usernames); i++){
	strcpy(name, current_usernames[i]);
	//if this username has already been used in this game, return -1
	if(strcmp(name, username)==0)
	return -1;
}
*/


//if the username is longer than permitted return -1
if(sizeof(username) > 30)
return -1;


while(username[username_index]!='\0' && username_index <= strlen(username)){
	l = username[username_index];
	fprintf(stderr, "letter at index %d = %c\n", username_index, l);
	//if the letter is not a number
	if(!(48<=l && 57>= l)){
		//and if the letter is not a capitol letter
		if(!(65<=l && 90>= l)){
			//and if the letter is not a lowercase letter
			if(!(97<= l && 122 >= l)){
				//if the username contains characters that are not alpha numberics return -1
				return -1;
			}
		}
	}
	username_index++;
}

//the username is valid return 1
#if DEBUG
printf("username %s is valid.\n", username);
#endif
return 1;
}


/* ---------------------------------------------------------------------------------*/
/* ----------------- NEED TO REDO THIS!!!!!!!! -------------------------------------*/
/* ---------------------------------------------------------------------------------*/




/*Input: input row, col val and gameboard.
*Output: 1 for valid 0 for invalid.
*Checks the sudoku block the users input is within. returns true if there
*are no duplicate values, else false.
*/
int blockIsValid(int row, int col, char val, char board[][BOARD_SIZE]){
	int block_row = (row-1) % 3;
	int block_col = (col-1) % 3;
	int i,j;

	#if DEBUG
	printf("block: row:%d, col: %d\n", block_row+1, block_col+1);
	#endif

	if(block_row == 0 && block_col == 0){
		for(i = 0; i<3; i++){
			for(j = 0; j<3; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}else if(block_row == 0 && block_col == 1){
		for(i = 0; i<3; i++){
			for(j = 3; j<6; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}else if(block_row == 0 && block_col == 2){
		for(i = 1; i<3; i++){
			for(j = 6; j<9; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}else if(block_row == 1 && block_col == 0){
		for(i = 3; i<6; i++){
			for(j = 0; j<3; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}else if(block_row == 1 && block_col == 1){
		for(i = 3; i<6; i++){
			for(j = 3; j<6; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}else if(block_row == 1 && block_col == 2){
		for(i = 3; i<6; i++){
			for(j = 6; j<9; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}else if(block_row == 2 && block_col == 0){
		for(i = 6; i<9; i++){
			for(j = 0; j<3; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}else if(block_row == 2 && block_col == 1){
		for(i = 6; i<9; i++){
			for(j = 3; j<6; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}else{
		for(i = 6; i<9; i++){
			for(j = 6; j<9; j++){
				if(board[i][j] == val)
				return 0;
			}
		}
	}
	return 1;
}
