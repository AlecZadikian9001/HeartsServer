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

// Game-related structs ===vv
typedef enum{
    suit_start = 0, // Error, not an actual value but an open bound.
    suit_hearts,
    suit_clubs,
    suit_diamonds,
    suit_spades,
    suit_end // Error, not an actual value but an open bound.
} Suit;
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
} Rank;
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
typedef int card; // card value = rank*RANK_PRIME + suit*SUIT_PRIME
#define MAX_NAME_LENGTH (16)
#define SEND_BUFFER_LEN (8 + MAX_CARDS_PER_PLAYER*3)
#define RECV_BUFFER_LEN (4 + MAX_NAME_LENGTH)

int main(int argc, const char * argv[]) {
    // Generate random name...v
    char name[8];
    srand (time(NULL));
    for (int i = 0; i<7; i++){
        name[i] = 'a' + rand()%20;
    }
    name[7] = '\0';
    printf("Starting RandomBot with name: %s\n", name);
    //...^
    // Create I/O named pipes, and open for reading and writing...v
    int fifoOut = mkfifo("/tmp/RandomBotOut", S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    int out = open("/tmp/RandomBotOut", O_RDONLY);
    int fifoIn = mkfifo("/tmp/RandomBotIn", S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    int in = open("/tmp/RandomBot", O_WRONLY);
    // ...^
    
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
    
    // General loop/communication variables...v
    unsigned char sendBuffer[SEND_BUFFER_LEN];
    unsigned char recvBuffer[RECV_BUFFER_LEN];
    size_t recvRet;
    size_t sendRet;
    int scanIndex;
    int cardIndex;
    bool endScan;
    char currentChar;
    bool running = true;
    //...^
    // Card variables...v
    card hand[MAX_CARDS_PER_PLAYER];
    memset(hand, NULL_CARD, MAX_CARDS_PER_PLAYER);
    int playerID; // This AI's player ID
    int numPlayers; // Number of players in game
    int turn; // Whose turn is it?
    int nextMove = -1; // -1 means no next move.
    //...^
    while (running){
        recvRet = cTalkRecv(in, recvBuffer, RECV_BUFFER_LEN);
        switch (recvBuffer[0]){
            case ']':{ // Card Played
                // Format: "]player,card"
                //TODO check to see what suit we need to play
                /*
                 YOUR CODE HERE
                 */
                break;
            }
            case '[':{ // Asking for Play
                // TODO just play a random, valid card
                break;
            }
            case ':':{ // New Round
                // Format: ":numPlayers,playerID,turn,card1,card2,card3,...cardN"
                numPlayers = atoi(recvBuffer+1);
                scanIndex=1;
                while (recvBuffer[scanIndex]!=',') scanIndex++;
                scanIndex++;
                playerID = atoi(recvBuffer+scanIndex);
                while (recvBuffer[scanIndex]!=',') scanIndex++;
                scanIndex++;
                turn = atoi(recvBuffer+scanIndex);
                cardIndex = 0;
                endScan = false;
                while (true){
                    while (recvBuffer[scanIndex]!=','){
                        currentChar = recvBuffer[scanIndex];
                        if (currentChar !='\0' && currentChar != ',' && (currentChar < '0' || currentChar > '9')){ // Error-checking
                            printf("***ERROR: RECEIVED INVALID MESSAGE FROM SERVER\n");
                            exit(1);
                        }
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
                
                // Start calculating the first move right now...v
                for (int i = 0; i<MAX_CARDS_PER_PLAYER; i++){ // Just see if we have the Two of Clubs.
                    if (hand[i]==makeCard(suit_clubs, rank_2)){
                        nextMove = i;
                        break;
                    }
                }
                //...^
                
                break;
            }
            case '@':{ // Asking for Name
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














