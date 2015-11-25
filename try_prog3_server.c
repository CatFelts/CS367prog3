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
#include <stdbool.h>
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

struct player {
	int sd;
	int score;
	char username[32];
	bool in_play;
};

void init_board(char board[][BOARD_SIZE]);
char *setup_board_message(char board[][BOARD_SIZE]);
char * setup_leaderboard_msg(struct player playersp[], char current_usernames[1024][32], int partic_sds[]);
void printboard(char board[][BOARD_SIZE]);
int moveIsValid(char move[], char board[][BOARD_SIZE]);
int usernameIsValid(const char* username, char current_usernames[1024][32]);
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

	//================/!Open Connection!/=================/
	bool game_in_play = true;
	char y_reply = 'Y';
	char n_reply = 'N';
	char used_usernames[1024][32]; /* keep an array of the participants usernames  -- NEED TO MAKE [][]*/
	struct player players[max_participants]; /*keep an array of current players(participants)*/
	int move_valid = -1;         /* store the validity of checking a participants move */
	int i = 0;
	int player_index = 0;
	int username_index = 0;
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
	}

	memset(players, '\0', sizeof(players));

	memset(used_usernames, 0, sizeof(used_usernames));



	//initial starting conditions
	pass_count = 0;
	init_board(game_board);
	char *board_msg = (char *)malloc(512 * sizeof(char) +2);
	//char board_msg[1024];
	board_msg = setup_board_message(game_board);
	char move_msg[1024];
	char full_msg[1024];
	char *leaderboard_msg = (char *)malloc((50* 5 + 12)* sizeof(char) + 1 + 1);
	char null_char[1];
	null_char[0] = '\0';


	#if DEBUG
	printf("%s\n",board_msg);
	#endif




	/* Main server loop - accept and handle requests
	/* game loop */
	while (1) {
		#if DEBUG
		printf("list of current usernames = \n");
		for(i = 0; i<max_participants; i++){
			if(players[i].in_play)
				printf("[%d] = %s\n", i, players[i].username);
		}
		#endif

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
			sd = players[i].sd;
			if( sd > 0 && players[i].in_play){
				FD_SET(sd, &read_fds);
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
		activity = select(max_sd +1, &read_fds, NULL, NULL, &stimeout);

		if(activity < 0){
			fprintf(stderr, "ERROR: select failed.\n");
			exit(EXIT_FAILURE);
		}
		else if(activity == 0){
			#if DEBUG
			fprintf(stderr, "select() timeout period expired.\n");
			#endif

			if(game_in_play){
				#if DEBUG
				fprintf(stderr, "!!!Round ended!!!\n");
				#endif
				stimeout.tv_sec = (time_t)K;
				game_in_play = false;

				//------------------send leaderboard to observers------------------
				//causes seg fault
				leaderboard_msg = setup_leaderboard_msg(players, used_usernames, participant_sds);

			}
			else if(!game_in_play){
				#if DEBUG
				fprintf(stderr, "!!!Inter-round ended. Start new round.!!!\n");
				#endif

				stimeout.tv_sec = (time_t)N;
				game_in_play = true;

				//-----------------------clear out gameboard------------------------
			}
		}
		else{


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
						username_valid = usernameIsValid(username, used_usernames);
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

					//make a new player
					struct player temp_player;
					temp_player.score = 0;
					temp_player.sd = sd2;
					strcpy(temp_player.username, username);
					temp_player.in_play = true;
					//add player to array of players;
					players[player_index] = temp_player;
					player_index++;

					//add new socket to array of sockets
					for(i = 0; i<sizeof(players); i++){
						//if position is empty
						if(participant_sds[i] == -1){
							participant_sds[i] = sd2;

							#if DEBUG
							fprintf(stderr, "adding participant_sds[%d]--> %d\n", i, participant_sds[i]);
							fprintf(stderr, "added %s player into list of players.\n", players[player_index -1].username);
							#endif

							break;
						}
					}

					#if DEBUG
					for(i = 0; i<max_participants; i++){
						fprintf(stderr, "participant_sds[%d]--> %d\n", i, participant_sds[i]);
					}
					#endif
				}
			}

			//else if something happened its an incoming move from a participant or the participant trying to close the connection
			for(i = 0; i<max_participants; i++){
				sd = players[i].sd;

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
						players[i].sd = -1;
						num_participants --;
					}

					else{
						//
						strcat(player_move, null_char);
						#if DEBUG
						printf("this is what was read from the participant: %s\n", player_move);
						#endif
						if(game_in_play){


							strcpy(full_msg, null_char);
							strcpy(move_msg, null_char);
							strcat(move_msg, players[i].username);

							//check to see if valid move
							if(move_valid = moveIsValid(player_move, game_board) == 1){
								//update gameboard accordingly
								game_board[(player_move[ROW]-64)-1][(player_move[COLUMN]-48)-1] = player_move[VALUE];
								//update players score
								players[i].score = players[i].score +1;
								board_msg = setup_board_message(game_board);
								#if DEBUG
								fprintf(stderr, "size of board_msg--> %lu\n", sizeof(board_msg));
								#endif
								strcat(move_msg, " made valid move ");
								strcat(move_msg, player_move);
								strcat(move_msg, "\n");
								#if DEBUG
								fprintf(stderr, "size of move_msg--> %lu\n", sizeof(move_msg));
								#endif
								strcat(full_msg, move_msg);
								strcat(full_msg, board_msg);


							}else{
								players[i].score = players[i].score -1;
								//make invalid move message
								strcat(move_msg, " attempted to make an invalid move.\n");
								strcat(full_msg, move_msg);

							}
							strcat(full_msg, null_char);

							#if DEBUG
							fprintf(stderr, "Size of full_msg--> %lu\n", sizeof(full_msg));
							fprintf(stderr, "%s\n", full_msg );
							#endif


							//send validity to observers
							for(i = 0; i<max_observers; i++){
								sd = observer_sds[i];

								//if this is a valid connected observer
								if(sd >0){
									#if DEBUG
									printf("sending move msg and game board msg to observer %d\n", sd);
									#endif
									//send full_msg
									if(send(sd, &full_msg, strlen(full_msg), 0) <= 0){
										fprintf((stderr), "Error: sending move_msg to observer %d\n", sd);
										exit(EXIT_FAILURE);
									}

								}

							}
						}
						else{
							#if DEBUG
							fprintf(stderr, "Player attempted move while game not in play\n");
							#endif
						}
					}
				}


			}
		}

		#if DEBUG
		for(i = 0; i < max_participants; i++){
			fprintf(stderr, "partic[%d] score ---> %d\n", i, players[i].score);
		}
		#endif

		#if DEBUG
		fprintf(stderr, "---------------end of cycle.-------------------\n");
		if(game_in_play)
			fprintf(stderr, "GAME IN PLAY.\n");
		else
			fprintf(stderr, "GAME NOT IN PLAY.\n");
		#endif

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
				strcat(msg, " ");
				strcat(msg, c_to_str);
			}
		}
		strcat(msg, " |");
	}
	strcat(msg, "\n--+-------+-------+-------+\n");
	#if DEBUG
	printf("%s\n", msg);
	#endif
	return msg;
}


char * setup_leaderboard_msg(struct player people[], char current_usernames[1024][32], int partic_sds[]){
	char * leaders = (char *)malloc((50* 5 + 12)* sizeof(char) + 1 + 1);
	leaders[0] = '\0';
	int i;
	int count = 0;
	int temp_score;
/*
	//find the top 5 scores
	int top_scores[5] = { scores[0], scores[0], scores[0], scores[0], scores[0]};
	int sds[5] = {-2, -2, -2, -2 , -2};

	for(i = 0; i<sizeof(partic_sds); i++){
		if(partic_sds[i] > 0)
			count++;
		temp_score = scores[i];
		if(temp_score > top_scores[0]){
			top_scores[4] = top_scores[3];
			top_scores[3] = top_scores[2];
			top_scores[2] = top_scores[1];
			top_scores[1] = top_scores[0];
			top_scores[0] = temp_score;
			sds[0] = partic_sds[i];
		}
		else if(temp_score > top_scores[1]){
			top_scores[4] = top_scores[3];
			top_scores[3] = top_scores[2];
			top_scores[2] = top_scores[1];
			top_scores[1] = temp_score;
			sds[1] = partic_sds[i];
		}
		else if(temp_score > top_scores[2]){
			top_scores[4] = top_scores[3];
			top_scores[3] = top_scores[2];
			top_scores[2] =  temp_score;
			sds[2] = partic_sds[i];
		}else if(temp_score > top_scores[3]){
			top_scores[4] = top_scores[3];
			top_scores[3] = temp_score;
			sds[3] = partic_sds[i];
		}else if(temp_score > top_scores[4]){
			top_scores[4] = temp_score;
			sds[4] = partic_sds[i];
		}
	}


	//build the message
	strcat(leaders, "Leaderboard:\n");

	for(i = 0; i<count; i++){
		if(i == 5)
			break;
		char place[3];
		sprintf(place, "%d) ", i+1);

		strcat(leaders, place);
		int sd = sds[i];
		#if DEBUG
		fprintf(stderr, "%s\n", current_usernames[sd]);
		#endif
		//strcat(leaders, current_usernames[sd]);
		//strcat(leaders, " has ");
		//char score[4];
		//sprintf(score, "%d", top_scores[i]);
		//strcat(leaders, score);
		//strcat(leaders, " points\n");

	}




	#if DEBUG
	fprintf(stderr, "Leaderboard msg----> \n%s\n", leaders);
	#endif
	*/



	return leaders;
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

	//if not row is not a captiol letter
	if(!(64 < move[0] && move[0] < 91))
	return 0;
	else if(!(48 < move[1] && move[1] < 58))
	return 0;
	else if(!(48 < move[2] && move[2] < 58))
	return 0;

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


int usernameIsValid(const char* username, char current_usernames[1024][32]){
	int username_index = 0;
	char l = username[username_index];
	char name[32];
	//char *name = (char *)malloc(30 * sizeof(char) +2);
	int i;


	for(i = 0; i<sizeof(current_usernames); i++){
		strcpy(name, current_usernames[i]);
		//if this username has already been used in this game, return -1
		if(strcmp(name, username)==0)
		return -1;
	}


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
