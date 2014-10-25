/*
 ███╗   ███╗ █████╗ ██╗███╗   ██╗    ██████╗
 ████╗ ████║██╔══██╗██║████╗  ██║   ██╔════╝
 ██╔████╔██║███████║██║██╔██╗ ██║   ██║
 ██║╚██╔╝██║██╔══██║██║██║╚██╗██║   ██║
 ██║ ╚═╝ ██║██║  ██║██║██║ ╚████║██╗╚██████╗
 ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝╚═╝ ╚═════╝
 */
//  HeartsServer
//
//  Created by Alec Zadikian on 10/23/14.
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
#include "ctalk.h"
#include "general.h"

// Game-related structs ===vv
typedef enum{
    suit_start = 0,
    suit_hearts,
    suit_clubs,
    suit_diamonds,
    suit_spades,
    suit_end
} Suit;
typedef enum{
    rank_start = 0,
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
    rank_end
} Rank;
#define NULL_CARD (0)
#define RANK_PRIME (2)
#define SUIT_PRIME (3)
typedef int card; // card value = rank*RANK_PRIME + suit*SUIT_PRIME // TODO fix all occurences where you get the suit or rank...
#define DECK_SIZE (52)
#define MAX_PLAYERS (5)
#define MAX_CARDS_PER_PLAYER (DECK_SIZE/MAX_PLAYERS)
struct Player{
    int fd; // file descriptor for AI comm
    uint64_t score;
    card hand[MAX_CARDS_PER_PLAYER];
    bool heartsVoid;
    bool clubsVoid;
    bool diamondsVoid;
    bool spadesVoid;
};
struct Game{
    struct Player players[MAX_PLAYERS];
    int numPlayers; // 3-5
    int turn; // Whose turn is it? This is an index in players.
    card trick[MAX_PLAYERS]; // cards on table
    int cardsPlayed; // number of cards played this trick
    int trickNo; // the trick number (starts at 0)
    bool heartsDropped; // Have hearts been dropped?
};
// ===^^

card makeCard(Suit suit, Rank rank){
    return SUIT_PRIME*suit + RANK_PRIME*rank;
}

ReturnCode shuffleDeck(card* deck, int numDiscarded){
    bzero(deck, sizeof(card)*DECK_SIZE);
    int index;
    bool isUp;
    for (Suit suit = suit_start+1; suit < suit_end; suit++){
        for (Rank rank = rank_start+1; rank < rank_end; rank++){
            if (suit == suit_clubs && rank!=2 && rank-2 <= numDiscarded) break;
            index = rand() % DECK_SIZE;
            if (deck[index]!=NULL_CARD){
                isUp = rand()%2;
                while (1){
                    index = (isUp?(index + 1):(index - 1)) % DECK_SIZE;
                    if (deck[index]==0) break;
                }
            }
            deck[index] = rank*RANK_PRIME + suit*SUIT_PRIME;
        }
    }
    return RET_NO_ERROR;
}

ReturnCode dealHand(struct Game* game){
    card deck[DECK_SIZE];
    int discarded = DECK_SIZE % game->numPlayers;
    shuffleDeck(deck, discarded);
    int deckIndex = 0;
    struct Player* player;
    int cardsPerPlayer = DECK_SIZE / game->numPlayers;
    for (int playerIndex = 0; playerIndex < game->numPlayers; playerIndex++){
        player = &game->players[playerIndex];
        for (int i = 0; i<cardsPerPlayer; i++){
            while (deck[deckIndex] == NULL_CARD) deckIndex++;
            player->hand[i] = deck[deckIndex];
        }
    }
    return RET_NO_ERROR;
}

ReturnCode handlePlay(struct Game* game, int cardIndex){
    struct Player* player = &game->players[game->turn];
    card playedCard = player->hand[cardIndex];
    if (player->hand[cardIndex] == NULL_CARD) return RET_INPUT_ERROR;
    player->hand[cardIndex] = NULL_CARD;
    // Validation...v
    if (game->trickNo == 0){
        if (game->cardsPlayed == 0 && playedCard != makeCard(suit_clubs, rank_2)) return RET_INPUT_ERROR; // first player must play 2 of clubs
        if (playedCard / SUIT_PRIME == suit_hearts || playedCard == makeCard(suit_spades, rank_Q)) return RET_INPUT_ERROR; // friendly trick; no hearts or queen of spades
    }
    if (!game->heartsDropped && game->cardsPlayed==0 && playedCard / SUIT_PRIME == suit_hearts) return RET_INPUT_ERROR; // no starting with hearts before hearts have been dropped
    if (game->cardsPlayed!=0){ // then we have to make sure the correct suit is being played
        Suit playedSuit = playedCard / SUIT_PRIME;
        Suit firstSuit = game->trick[0] / SUIT_PRIME;
        if (playedSuit != firstSuit){ // if the client is trying to claim a void
            bool excused = false;
            switch (playedSuit){ // maybe the void is already cached
                case suit_clubs:{ excused = player->clubsVoid; break; }
                case suit_hearts:{ excused = player->heartsVoid; break; }
                case suit_diamonds:{ excused = player->diamondsVoid; break; }
                case suit_spades:{ excused = player->spadesVoid; break; }
            }
            if (!excused){ // if it's not, we have to check the player's hand
                for (int i = 0 ; i<MAX_CARDS_PER_PLAYER; i++){
                    if (player->hand[i] / SUIT_PRIME == firstSuit) return RET_INPUT_ERROR; // tried to illegally claim void
                }
                switch (firstSuit){ // cache it if it's valid
                    case suit_clubs:{ player->clubsVoid = true; break; }
                    case suit_hearts:{ player->heartsVoid = true; break; }
                    case suit_diamonds:{ player->diamondsVoid = true; break; }
                    case suit_spades:{ player->spadesVoid = true; break; }
                }
            }
        }
    }
    //...^
    game->cardsPlayed++;
    if (playedCard / SUIT_PRIME == suit_hearts || playedCard == makeCard(suit_spades, rank_Q)) game->heartsDropped = true;
    if (game->cardsPlayed == game->numPlayers){
        
    }
}

int main(int argc, const char * argv[]) {
    
    return 0;
}



























