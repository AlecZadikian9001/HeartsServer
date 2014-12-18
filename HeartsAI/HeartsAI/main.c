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
// Communcation constants ===vv
#define MAX_NAME_LENGTH (16)
#define SEND_BUFFER_LEN (8 + MAX_CARDS_PER_PLAYER*3)
#define RECV_BUFFER_LEN (4 + MAX_NAME_LENGTH)

int main(int argc, const char * argv[]) {
    // insert code here...
    int fifo = mkfifo("/tmp/RandomBot", S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    int fd = open("/tmp/RandomBot", O_WRONLY);
    char name[8];
    for (int i = 0; i<8; i++){
        name[i] = 'a' + rand()%4;
    }
    printf("Starting RandomBot with name: %s\n", name);
    
    return 0;
}














