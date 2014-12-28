//
//  main.c
//  HeartsAI
//
//  Created by Alec Zadikian on 12/18/14.
//  Copyright (c) 2014 AlecZ. All rights reserved.
//

#include <stdio.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ctalk.h"
#include "cJSON.h"
#include "general.h"

// Game-related definitions ===vv
typedef enum{
    suit_start = 0, // Error, not an actual value but an open bound.
    suit_hearts,
    suit_clubs,
    suit_diamonds,
    suit_spades,
    suit_end // Error, not an actual value but an open bound.
} suit;
typedef enum{
    rank_start = 0, // Error, not an actual value but an open bound.
    rank_2,
    rank_3,
    rank_4,
    rank_5,
    rank_6,
    rank_7,
    rank_8,
    rank_9,
    rank_10,
    rank_J,
    rank_Q,
    rank_K,
    rank_A,
    rank_end // Error, not an actual value but an open bound.
} rank;
#define NULL_CARD (0)
#define RANK_BASE (1)
#define SUIT_BASE (rank_end)
#define suitOf(card) ((int) (card / SUIT_BASE))
#define rankOf(card) (card % SUIT_BASE)
#define makeCard(suit, rank) (SUIT_BASE*RANK_BASE*suit + RANK_BASE*rank)
#define isValidCard(card) (suitOf(card) > suit_start && suitOf(card) < suit_end && rankOf(card) > rank_start && rankOf(card) < rank_end)
#define DECK_SIZE (52)
#define MIN_PLAYERS (3)
#define MAX_PLAYERS (6)
#define MAX_CARDS_PER_PLAYER (DECK_SIZE/MIN_PLAYERS)
typedef int card;
#define MAX_NAME_LENGTH (16)
#define SEND_BUFFER_LEN (4 + MAX_NAME_LENGTH+20) // TODO sloppy overestimation
#define RECV_BUFFER_LEN (8 + MAX_CARDS_PER_PLAYER*3+20) // TODO sloppy overestimation
// ===^^

char* nameOfSuit(suit suit){
    if (suit==suit_clubs) return "clubs";
    if (suit==suit_hearts) return "hearts";
    if (suit==suit_spades) return "spades";
    if (suit==suit_diamonds) return "diamonds";
    if (suit==suit_start) return "START_ERROR";
    return "END_ERROR";
}

/*
 getName() and handlePlay() are the only two functions you will need to edit to create your AI.
 You may also need some persistent variables for your AI's calculations. For simplicity, use global variables (even though they're "evil").
 And edit the language selection #defines if you aren't going to use C.
 It should be doable just by editing the code within the "YOUR CODE HERE" commented areas.
 
 vvv
 */

// Alternate language selection (make EXACTLY one of these true if you aren't using C)...v
/* v EDIT CODE HERE v */
#define USING_PYTHON (false) // If you're using Python (in which case Python scripts will be called directly from the C template).
#define USING_OTHER (false) // If you're using Java or something else that supports FIFO pipes and can parse JSON.
/* ^ EDIT CODE HERE ^ */
#ifdef USING_OTHER
#define INTERPROCESS_BUFLEN (2048) // TODO make sure this is a suitable buffer size
char interProcessInBuffer[INTERPROCESS_BUFLEN];
char interProcessReadBuffer[INTERPROCESS_BUFLEN];
char interProcessOutBuffer[INTERPROCESS_BUFLEN];
size_t interProcessPipeBytesAvailable = 0;
#endif
//...^

// Global variables...v
int interProcessOutFD = -1;
int interProcessInFD = -1;
/* v YOUR CODE HERE v */

/* ^ YOUR CODE HERE ^ */
//...^

int handlePlayUsingFIFO(int* playerID, int* numPlayers, int* firstPlayer, int* turn, int* winner, bool* heartsBroken, card* currentCard, suit* currentSuit, rank* highestRank, card* hand, card* trick, int* trickNo){ // For passing input to Java or other process. Do not edit.
    
    bool isItMyTurn = (handlePlay(playerID, numPlayers, firstPlayer, turn, winner, heartsBroken, currentCard, currentSuit, highestRank, hand, trick, trickNo) == true);
    
    if (interProcessOutFD == -1 || interProcessInFD == -1){ // Open the pipes first
        bool error = false;
        char* inPath = "/tmp/heartsIn";
        char* outPath = "/tmp/heartsOut";
        int fifoOut = mkfifo(outPath, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
        int fifoIn = mkfifo(inPath, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
        int in = open(inPath, O_RDONLY);
        if (in==-1){
            printf("***ERROR OPENING IN FILE, STOPPING***\n");
            error = true;
        }
        else printf("Opened interprocess AI logic in pipe at %s\n", inPath);
        int out = open(outPath, O_WRONLY);
        if (out==-1){
            printf("***ERROR OPENING OUT FILE, STOPPING***\n");
            error = true;
        }
        else printf("Opened interprocess AI logic out pipe at %s\n", outPath);
        if (error) exit(1);
    }
    
    cJSON* mainJSON = cJSON_CreateObject();
    cJSON* handJSON = cJSON_CreateIntArray(hand, MAX_CARDS_PER_PLAYER);
    cJSON* trickJSON = cJSON_CreateIntArray(trick, MAX_PLAYERS);
    cJSON_AddItemToObject(mainJSON, "hand", handJSON);
    cJSON_AddItemToObject(mainJSON, "trick", trickJSON);
    cJSON_AddNumberToObject(mainJSON, "playerID", *playerID);
    cJSON_AddNumberToObject(mainJSON, "numPlayers", *numPlayers);
    cJSON_AddNumberToObject(mainJSON, "firstPlayer", *firstPlayer);
    cJSON_AddNumberToObject(mainJSON, "turn", *turn);
    cJSON_AddNumberToObject(mainJSON, "winner", *winner);
    cJSON_AddBoolToObject(mainJSON, "heartsBroken", *heartsBroken);
    cJSON_AddNumberToObject(mainJSON, "currentCard", *currentCard);
    cJSON_AddNumberToObject(mainJSON, "currentSuit", *currentSuit);
    cJSON_AddNumberToObject(mainJSON, "highestRank", *highestRank);
    cJSON_AddNumberToObject(mainJSON, "trickNo", *trickNo);
    
    char* printedJSON = cJSON_Print(mainJSON);
    strcpy(interProcessOutBuffer, printedJSON);
    cJSON_Delete(mainJSON);
    efree(printedJSON);
    size_t len = strlen(interProcessOutBuffer);
    interProcessOutBuffer[len+1] = '\4'; // '\4' marks the end of a message
    printf("Sending data over pipe to AI process:\n%s\n... of length %lu\n", interProcessOutBuffer, len+1);
    write(interProcessOutFD, interProcessOutBuffer, len+2);
    size_t recv = 0;
    bool breakOut = false;
    while (true){ // Read until '/4', shift rest of data in buffer down
        for (size_t i = 0; i<interProcessPipeBytesAvailable; i++){
            if (interProcessInBuffer[i] == '\4'){
                strcpy(interProcessReadBuffer, interProcessInBuffer);
                memcpy(interProcessInBuffer, interProcessInBuffer+i+1, interProcessPipeBytesAvailable - i);
                interProcessPipeBytesAvailable -= (i + 1);
                breakOut = true;
                break;
            }
        }
        if (breakOut) break;
        else{
            recv = read(interProcessInFD, interProcessInBuffer + interProcessPipeBytesAvailable, INTERPROCESS_BUFLEN - interProcessPipeBytesAvailable);
            interProcessPipeBytesAvailable += recv;
        }
    }
    printf("Received message from pipe from AI process:\n%s\n... of length %lu\n", interProcessReadBuffer, strlen(interProcessReadBuffer)+1);
    
    cJSON* receivedJSON = cJSON_Parse(interProcessReadBuffer);
    // TODO parse received JSON, updating pointers
    // TODO return card to play
    
    // TODO Make Java template
}

char* getName(){ // Return the bot's name, must be unique, must be null-terminated
    char* name;
    
    /* v YOUR CODE HERE (REPLACE DEFAULT) v */
    // Generates a random name...v
    name = emalloc(MAX_NAME_LENGTH);
    srand((unsigned int)time(NULL));
    for (int i = 0; i<MAX_NAME_LENGTH-1; i++){
        name[i] = 'a' + rand()%20;
    }
    //...^
    /* ^ YOUR CODE HERE (REPLACE DEFAULT) ^ */
    
    name[MAX_NAME_LENGTH-1] = '\0';
    return name;
}

int handlePlay(int* playerID, int* numPlayers, int* firstPlayer, int* turn, int* winner, bool* heartsBroken, card* currentCard, suit* currentSuit, rank* highestRank, card* hand, card* trick, int* trickNo){ // Called every time a card is played by anyone, including the AI. If the AI decides to play a different card based on this, it returns its index in the hand. Otherwise returns -1.
    // Setup (do not edit)...v
    if (*turn == *firstPlayer){ // New trick
        *currentSuit = suitOf(*currentCard);
        *highestRank = rank_start;
        memset(trick, NULL_CARD, MAX_PLAYERS*sizeof(card)); // Reset the trick array
    }
    trick[*turn] = *currentCard;
    if (suitOf(*currentCard)==suit_hearts || *currentCard == makeCard(suit_spades, rank_Q)) *heartsBroken = true;
    if (suitOf(*currentCard) == *currentSuit && rankOf(*currentCard) > *highestRank){
        *highestRank = rankOf(*currentCard);
        *winner = *turn;
    }
    int temp1 = *firstPlayer-1;
    if (temp1<0) temp1+=*numPlayers;
    bool isItMyTurn = false;
    if (*turn == temp1){ // Last card in trick
        *firstPlayer = *winner; // Set the first player of the next trick to the winner
        if (*firstPlayer == *playerID) isItMyTurn = true;
    }
    int playerBeforeMe = *playerID-1;
    if (playerBeforeMe < 0) playerBeforeMe += *numPlayers;
    if (playerBeforeMe == *turn) isItMyTurn = true;
    
    if (USING_PYTHON || USING_OTHER) return isItMyTurn;
    //printf("Player %d just played the %d of %s, player %d owns the trick, player %d went first, the trick suit is %s, and the highest card is a %d.\n", *turn, rankOf(*currentCard)+1, nameOfSuit(suitOf(*currentCard)), *winner, *firstPlayer, nameOfSuit(*currentSuit), *highestRank+1);
    //...^
    
    /*
     This is where you write your AI's logic. This function will be called each turn in order.
     By the end of the code you write, int ret will represent the index of the card that you want to play (-1 if not decided yet).
     At least one of the calls (not necessarily the last) of handlePlay must return a card index (i.e. not -1) before it is the player's turn.
     
     At this point, you have the following pointers to variables work with when making your AI's logic:
     - bool isItMyTurn: Will it be your AI's turn next? If so, and if you haven't already chosen a move, you need to choose one now.
     - int* playerID: Your AI's player ID
     - int* turn: The ID fo the player who just went
     - int* firstPlayer: The ID of the player who began the trick
     - card* currentCard: The card that was just played
     - suit* currentSuit: The suit of the trick
     - rank* highestRank: The highest rank that has been played ON SUIT this round
     - int* winner: The ID of the player who currently has played the highest card on suit in the trick
     - bool* heartsBroken: Whether or not hearts have been broken this round
     - card* hand: Your AI's hand, an array of cards
     - card* trick: The trick (aka the cards on the table), an array of cards
     - int* trickNo: How many tricks have been played already in this round.
     */
    
    int ret = -1;
    
    /* v YOUR CODE HERE (REPLACE DEFAULT) v */
    // Play _any_ valid card in the hand if it's our turn next...v
    if (isItMyTurn){
        bool isVoid = false;
        bool rareHeartsCase = false;
        card checkCard;
        for (int i = 0; i<MAX_CARDS_PER_PLAYER; i++){
            checkCard = hand[i];
            if (checkCard!=NULL_CARD){
                if (*firstPlayer == *playerID){
                    //printf("This is a new hand.\n");
                    if (*heartsBroken==true || suitOf(checkCard)!=suit_hearts || rareHeartsCase == true){ ret = i; break; }
                }
                else{
                    if (isVoid){ ret = i; break; }
                    if (suitOf(checkCard) == *currentSuit){ ret = i; break; }
                }
            }
            if (i == MAX_CARDS_PER_PLAYER-1){
                if (rareHeartsCase){
                    printf("***CAN'T FIND A VALID CARD TO PLAY! RAGEQUITTING!***\n");
                    printf("Player %d just played the %d of %s, player %d owns the trick, player %d went first, the trick suit is %s, heartsBroken = %d and the highest card is a %d.\n", *turn, rankOf(*currentCard)+1, nameOfSuit(suitOf(*currentCard)), *winner, *firstPlayer, nameOfSuit(*currentSuit), *heartsBroken, *highestRank+1);
                    printf("Trick: ");
                    for (int i = 0; i<MAX_PLAYERS; i++){
                        printf("%d of %s, ", rankOf(trick[i])+1, nameOfSuit(suitOf(trick[i])));
                    }
                    printf("\n");
                    printf("Hand: ");
                    for (int i = 0; i<MAX_CARDS_PER_PLAYER; i++){
                        printf("%d of %s, ", rankOf(hand[i])+1, nameOfSuit(suitOf(hand[i])));
                    }
                    printf("\n");
                    exit(1);
                }
                if (isVoid){
                    // This is a rare case. We're void on everything but Hearts, but Hearts aren't broken, and we need to lead.
                    rareHeartsCase = true;
                }
                isVoid = true;
                i = -1;
            }
        }
    }
    //...^
    /* ^ YOUR CODE HERE (REPLACE DEFAULT) ^ */
    
    if (*turn == temp1){ // Last card in trick
        *turn = *winner;
        *trickNo = *trickNo + 1;
    }
    
    if (ret!=-1){
        //printf("Your AI has chosen to play %d (index %d) (the %d of %s)...\n", hand[ret], ret, rankOf(hand[ret])+1, nameOfSuit(suitOf(hand[ret])));
        //if (!isValidCard(hand[ret])) printf("***YOUR AI IS PLAYING A NULL CARD; GG no re, expect a crash soon.***\n");
    }
    else{
        //printf("Your AI has not chosen which card it wants to play yet...\n");
        if (isItMyTurn) printf("***IT'S ABOUT TO BE YOUR TURN, AND YOU HAVE NOT CHOSEN A CARD TO PLAY! GG no re; expect a crash soon.***\n");
    }
    return ret;
}

/*
 ^^^
 
 getName() and handlePlay() are the only two functions you will need to edit to create your AI.
 It should be doable just by editing the code within the "YOUR CODE HERE" commented areas.
 */

int main(int argc, const char * argv[]) {
    logLevel = LOG_ERROR;
    // Get name...v
    char* name;
    if (argc==2) name = argv[1];
    else name = getName();
    printf("\n=================================\nStarting AI with name %s\n=================================\n\n", name);
    //...^
    // Create I/O named pipes, and open for reading and writing...v
    bool error = false;
    char inPath[MAX_NAME_LENGTH+strlen("/tmp/Out")+20];
    char outPath[MAX_NAME_LENGTH+strlen("/tmp/Out")+20];
    sprintf(inPath, "/tmp/%sIn", name);
    sprintf(outPath, "/tmp/%sOut", name);
    int fifoOut = mkfifo(outPath, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    int fifoIn = mkfifo(inPath, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    int in = open(inPath, O_RDONLY);
    if (in==-1){
        printf("***ERROR OPENING IN FILE, STOPPING***\n");
        error = true;
    }
    else printf("Opened in pipe at %s\n", inPath);
    int out = open(outPath, O_WRONLY);
    if (out==-1){
        printf("***ERROR OPENING OUT FILE, STOPPING***\n");
        error = true;
    }
    else printf("Opened out pipe at %s\n", outPath);
    if (error) exit(1);
    //...^
    
    /*
     Reference of message headers:{
     _Message Header_               _Meaning_				_Action_
     "@"							Send name				ASAP send the name of your AI.
     ":"							Round starting			Read in your hand, set up your AI, etc.
     "["							Asking for play			ASAP send the index of the card you want to play.
     "]"							Card played				Take note that that player played that card.
     ";"							Testing ended			Shut down your AI.
     }
     */
    
    // General loop/communication variables, some pre-allocated because we don't want to have the inefficiency of allocating new variables in the while(running) loop below...v
    unsigned char sendBuffer[SEND_BUFFER_LEN];
    unsigned char recvBuffer[RECV_BUFFER_LEN];
    size_t recvRet;
    size_t sendRet;
    int scanIndex;
    int cardIndex;
    int temp1;
    int temp2;
    bool endScan;
    char currentChar;
    bool running = true;
    //...^
    // Card variables...v
    card hand[MAX_CARDS_PER_PLAYER]; // Cards in your AI's hand.
    card trick[MAX_PLAYERS]; // Cards on the table (in order of player ID).
    card currentCard =  NULL_CARD; // The last card that was played.
    int trickNo =       -1; // How many tricks have been played this round.
    int playerID =      -1; // This AI's player ID
    int numPlayers =    -1; // Number of players in game
    int firstPlayer =   -1; // Who goes first in this trick?
    int turn =          -1; // Whose turn is it?
    int nextMove =      -1; // Next move the AI intends to play. -1 means no next move has been chosen yet.
    int winner =        -1; // The player who is (thus far) set to take the trick.
    bool heartsBroken = false; // Have hearts been broken this round?
    suit currentSuit = suit_start; // The suit of the current trick.
    rank highestRank = rank_start; // The highest rank on suit that has been played during this trick.
    //...^
    while (running){
        recvRet = cTalkRecv(in, recvBuffer, RECV_BUFFER_LEN);
        if (recvRet==0 || recvRet==-1){
            //printf("**ERROR RECEIVING DATA, STOPPING**\n");
            //exit(1);
            //usleep(5000); // TODO Fix this so it blocks properly!
        }
        else{
            switch (recvBuffer[0]){
                case ']':{ // Card Played
                    // Format: "]player,card"
                    //printf("Server is notifying AI of play.\n");
                    turn = atoi(recvBuffer+1);
                    scanIndex = 1;
                    while (recvBuffer[scanIndex]!=',') scanIndex++;
                    scanIndex++;
                    currentCard = atoi(recvBuffer+scanIndex);
                    temp1 = handlePlay(&playerID, &numPlayers, &firstPlayer, &turn, &winner, &heartsBroken, &currentCard, &currentSuit, &highestRank, hand, trick, &trickNo);
                    if (temp1!=-1) nextMove = temp1;
                    break;
                }
                case '[':{ // Asking for Play
                    if (nextMove==-1){
                        printf("***ERROR: It is your AI's turn, and it has not chosen a move. Ragequitting.***\n");
                        printf("Player %d just played the %d of %s, player %d owns the trick, player %d went first, the trick suit is %s, heartsBroken = %d, the highest card is a %d, and trickNo is %d.\n", turn, rankOf(currentCard)+1, nameOfSuit(suitOf(currentCard)), winner, firstPlayer, nameOfSuit(currentSuit), heartsBroken, highestRank+1, trickNo);
                        printf("recvBuffer: %s", recvBuffer);
                        printf("Trick: ");
                        for (int i = 0; i<MAX_PLAYERS; i++){
                            printf("%d of %s, ", rankOf(trick[i])+1, nameOfSuit(suitOf(trick[i])));
                        }
                        printf("\n");
                        printf("Hand: ");
                        for (int i = 0; i<MAX_CARDS_PER_PLAYER; i++){
                            printf("%d of %s, ", rankOf(hand[i])+1, nameOfSuit(suitOf(hand[i])));
                        }
                        printf("\n");
                        exit(1);
                    }
                    if (!isValidCard(hand[nextMove])){
                        printf("***YOUR AI IS PLAYING AN INVALID CARD AT INDEX %d. EXPECT A CRASH.***\n", nextMove);
                    }
                    //printf("You play %d (index %d) (the %d of %s).\n", hand[nextMove], nextMove, rankOf(hand[nextMove])+1, nameOfSuit(suitOf(hand[nextMove])));
                    sprintf(sendBuffer, "%d", nextMove);
                    cTalkSend(out, sendBuffer, strlen(sendBuffer)+1);
                    currentCard = hand[nextMove];
                    hand[nextMove] = NULL_CARD;
                    nextMove = -1;
                    turn = playerID;
                    temp1 = handlePlay(&playerID, &numPlayers, &firstPlayer, &turn, &winner, &heartsBroken, &currentCard, &currentSuit, &highestRank, hand, trick, &trickNo);
                    if (temp1!=-1) nextMove = temp1;
                    break;
                }
                case ':':{ // New Round
                    // Format: ":numPlayers,playerID,turn,card1,card2,card3,...cardN"
                    currentCard =  NULL_CARD; // The last card that was played.
                    trickNo =       -1; // How many tricks have been played this round.
                    turn =          -1; // Whose turn is it?
                    nextMove =      -1; // Next move the AI intends to play. -1 means no next move has been chosen yet.
                    winner =        -1; // The player who is (thus far) set to take the trick.
                    heartsBroken = false; // Have hearts been broken this round?
                    currentSuit = suit_start; // The suit of the current trick.
                    highestRank = rank_start; // The highest rank on suit that has been played during this trick.
                    // Read input...v
                    //printf("Server is starting new round.\n");
                    numPlayers = atoi(recvBuffer+1);
                    scanIndex=1;
                    while (recvBuffer[scanIndex]!=',') scanIndex++;
                    scanIndex++;
                    playerID = atoi(recvBuffer+scanIndex);
                    while (recvBuffer[scanIndex]!=',') scanIndex++;
                    scanIndex++;
                    turn = atoi(recvBuffer+scanIndex);
                    firstPlayer = turn;
                    heartsBroken = false;
                    memset(trick, NULL_CARD, MAX_PLAYERS*sizeof(card)); // Reset the trick array
                    cardIndex = 0;
                    endScan = false;
                    //printf("Your hand: ");
                    while (true){
                        while (recvBuffer[scanIndex]!=','){
                            currentChar = recvBuffer[scanIndex];
                         /*   if (currentChar !='\0' && currentChar != ',' && (currentChar < '0' || currentChar > '9')){ // Error-checking
                                printf("***ERROR: RECEIVED INVALID MESSAGE FROM SERVER\n");
                                exit(1);
                            } */
                            if (currentChar=='\0'){
                                endScan = true;
                                break;
                            }
                            scanIndex++;
                        }
                        if (endScan) break;
                        scanIndex++;
                        hand[cardIndex] = atoi(recvBuffer+scanIndex);
                        //printf("%d of %s, ", rankOf(hand[cardIndex])+1, nameOfSuit(suitOf(hand[cardIndex])));
                        cardIndex++;
                    }
                    //printf("\n");
                    for (; cardIndex<MAX_CARDS_PER_PLAYER; cardIndex++){
                        hand[cardIndex] = NULL_CARD;
                    }
                    //...^
                    trickNo = 0;
                    for (int i = 0; i<MAX_CARDS_PER_PLAYER; i++){ // See if we have the Two of Clubs.
                        if (hand[i]==makeCard(suit_clubs, rank_2)){
                            nextMove = i;
                            break;
                        }
                    }
                    
                    break;
                }
                case '^':{ //Ping
                    cTalkSend(out, "^", 2);
                    printf("Server pinged.\n");
                }
                case '@':{ // Asking for Name
                    printf("Server connected and asked for name.\n");
                    sendRet = cTalkSend(out, name, strlen(name)+1); // Send the name.
                    break;
                }
                case ';':{ // End of Tests
                    printf("Tests are over. Exiting.\n");
                    exit(0);
                    break;
                }
            }
        }
    }
    
    return 0;
}














