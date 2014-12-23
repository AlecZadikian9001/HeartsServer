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
#define DECK_SIZE (52)
#define MIN_PLAYERS (3)
#define MAX_PLAYERS (6)
#define MAX_CARDS_PER_PLAYER (DECK_SIZE/MAX_PLAYERS)
typedef int card;
#define MAX_NAME_LENGTH (16)
#define SEND_BUFFER_LEN (8 + MAX_CARDS_PER_PLAYER*3)
#define RECV_BUFFER_LEN (4 + MAX_NAME_LENGTH)
// ===^^

/*
 getName() and handlePlay() are the only two functions you will need to edit to create your AI.
 It should be doable just by editing the code within the "YOUR CODE HERE" commented areas.
 
 vvv
 */

char* getName(){ // Return the bot's name, must be unique, must be null-terminated
    char* name = emalloc(MAX_NAME_LENGTH);
    
    /* v YOUR CODE HERE (REPLACE DEFAULT) v */
    // Generates a random name...v
    srand((unsigned int)time(NULL));
    for (int i = 0; i<MAX_NAME_LENGTH-1; i++){
        name[i] = 'a' + rand()%20;
    }
    //...^
    /* ^ YOUR CODE HERE (REPLACE DEFAULT) ^ */
    
    name[MAX_NAME_LENGTH-1] = '\0';
    return name;
}

int handlePlay(int* playerID, int* numPlayers, int* firstPlayer, int* turn, int* winner, bool* heartsBroken, card* currentCard, suit* currentSuit, rank* highestRank, card** hand, card** trick){ // Called every time a card is played by anyone, including the AI. If the AI decides to play a different card based on this, it returns its index in the hand. Otherwise returns -1.
    // Setup (do not edit)...v
    if (turn == firstPlayer){ // New trick
        *currentSuit = suitOf(*currentCard);
        *highestRank = rank_start;
        memset(trick, NULL_CARD, MAX_PLAYERS*sizeof(card)); // Reset the trick array
    }
    *trick[*turn] = *currentCard;
    if (suitOf(*currentCard)==suit_hearts) *heartsBroken = true;
    if (rankOf(*currentCard) > *highestRank){
        *highestRank = rankOf(*currentCard);
        *winner = *turn;
    }
    int temp1 = *firstPlayer-1;
    if (temp1<0) temp1+=*numPlayers;
    if (*turn == temp1){ // Last card in trick
        *firstPlayer = *winner; // Set the first player of the next trick to the winner
    }
    //...^
    
    /*
     This is where you write your AI's logic. This function will be called each turn in order.
     At least one of the calls of handlePlay must return a card index (i.e. not -1) before it is the player's turn.
     
     At this point, you have the following pointers to variables work with when making your AI's logic:
     - int* playerID: Your AI's player ID
     - int* turn: The ID fo the player who just went
     - int* firstPlayer: The ID of th e player who began the trick
     - card* currentCard: The card that was just played
     - suit* currentSuit: The suit of the trick
     - rank* highestRank: The highest rank that has been played ON SUIT this round
     - int* winner: The ID of the player who currently has played the highest card on suit in the trick
     - bool* heartsBroken: Whether or not hearts have been broken this round
     - card** hand: Your AI's hand, an array of cards
     - card** trick: The trick (aka the cards on the table), an array of cards
     */
    
    /* v YOUR CODE HERE (REPLACE DEFAULT) v */
    // Play _any_ valid card in the hand...v
    srand((unsigned int)time(NULL));
    card checkCard;
    int ret = -1;
    for (int i = 0; i<MAX_CARDS_PER_PLAYER; i++){
        checkCard = *hand[i];
        if (checkCard!=NULL_CARD){
            if (*firstPlayer == *playerID){
                if (!heartsBroken || suitOf(checkCard)!=suit_hearts){ ret = i; break; }
            }
            else{
                if (suitOf(checkCard)==*currentSuit){ ret = i; break; }
                else if (i == MAX_CARDS_PER_PLAYER-1 || hand[i+1] == NULL_CARD){ // We're void.
                    ret = i; break;
                }
            }
        }
    }
    //...^
    /* ^ YOUR CODE HERE (REPLACE DEFAULT) ^ */
    
    return -1;
}

/*
 ^^^
 
 getName() and handlePlay() are the only two functions you will need to edit to create your AI.
 It should be doable just by editing the code within the "YOUR CODE HERE" commented areas.
 */

int main(int argc, const char * argv[]) {
    logLevel = LOG_FULL;
    // Get name...v
    char* name = getName();
    printf("Starting AI with name: %s\n", name);
    //...^
    // Create I/O named pipes, and open for reading and writing...v
    bool error = false;
    char inPath[MAX_NAME_LENGTH+strlen("/tmp/Out")+20];
    char outPath[MAX_NAME_LENGTH+strlen("/tmp/Out")+20];
    sprintf(inPath, "/tmp/%sIn", name);
    sprintf(outPath, "/tmp/%sOut", name);
    int fifoOut = mkfifo(outPath, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    int out = open(outPath, O_WRONLY);
    if (out==-1){
        printf("***ERROR OPENING OUT FILE, STOPPING***\n");
        error = true;
    }
    else printf("Opened out pipe at %s\n", outPath);
    int fifoIn = mkfifo(inPath, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    int in = open(inPath, O_RDONLY);
    if (in==-1){
        printf("***ERROR OPENING IN FILE, STOPPING***\n");
        error = true;
    }
    else printf("Opened in pipe at %s\n", inPath);
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
    card currentCard;
    bool endScan;
    char currentChar;
    bool running = true;
    //...^
    // Card variables...v
    card hand[MAX_CARDS_PER_PLAYER]; // Cards in your AI's hand.
    card trick[MAX_PLAYERS]; // Cards on the table (in order of player ID).
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
        switch (recvBuffer[0]){
            case ']':{ // Card Played
                // Format: "]player,card"
                turn = atoi(recvBuffer+1);
                scanIndex = 1;
                while (recvBuffer[scanIndex]!=',') scanIndex++;
                scanIndex++;
                currentCard = atoi(recvBuffer+scanIndex);
                temp1 = handlePlay(&playerID, &numPlayers, &firstPlayer, &turn, &winner, &heartsBroken, &currentCard, &currentSuit, &highestRank, &hand, &trick); // TODO verify type castings
                if (temp1!=-1) nextMove = temp1;
                break;
            }
            case '[':{ // Asking for Play
                sprintf(sendBuffer, "%d", nextMove);
                cTalkSend(out, sendBuffer, strlen(sendBuffer)+1);
                turn = playerID;
                currentCard = hand[nextMove];
                temp1 = handlePlay(&playerID, &numPlayers, &firstPlayer, &turn, &winner, &heartsBroken, &currentCard, &currentSuit, &highestRank, &hand, &trick); // TODO verify type castings
                if (temp1!=-1) nextMove = temp1;
                break;
            }
            case ':':{ // New Round
                // Format: ":numPlayers,playerID,turn,card1,card2,card3,...cardN"
                
                // Read input...v
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
                    cardIndex++;
                }
                for (; cardIndex<MAX_CARDS_PER_PLAYER; cardIndex++){
                    hand[cardIndex] = NULL_CARD;
                }
                //...^
                
                for (int i = 0; i<MAX_CARDS_PER_PLAYER; i++){ // See if we have the Two of Clubs.
                    if (hand[i]==makeCard(suit_clubs, rank_2)){
                        nextMove = i;
                        break;
                    }
                }
                
                break;
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
    
    return 0;
}














