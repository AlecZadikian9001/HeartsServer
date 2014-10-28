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
#define suitOf(card) (card % RANK_PRIME)
#define rankOf(card) (card % SUIT_PRIME)
#define makeCard(suit, rank) (SUIT_PRIME*suit + RANK_PRIME*rank)
typedef int card; // card value = rank*RANK_PRIME + suit*SUIT_PRIME
#define DECK_SIZE (52)
#define MAX_PLAYERS (5)
#define MAX_CARDS_PER_PLAYER (DECK_SIZE/MAX_PLAYERS)
#define SEND_BUFFER_LEN (8 + MAX_CARDS_PER_PLAYER*3)
#define RECV_BUFFER_LEN (4)
struct Player{
    int fd; // file descriptor for AI comm
    char sendBuffer[SEND_BUFFER_LEN];
    char recvBuffer[RECV_BUFFER_LEN];
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
    int deckSize; // How many cards in deck?
    int turn; // Whose turn is it? This is an index in players.
    int winner; // Player who is currently poised to take the trick.
    card trick[MAX_PLAYERS]; // cards on table
    int cardsPlayed; // number of cards played this trick
    int trickNo; // the trick number (starts at 0)
    bool heartsDropped; // Have hearts been dropped?
};
// ===^^

// Communcation constants ===vv

// ===^^

ReturnCode handlePlay(struct Game* game, int cardIndex){
    struct Player* player = &game->players[game->turn];
    card playedCard = player->hand[cardIndex];
    Suit playedSuit = suitOf(playedCard);
    Rank playedRank = rankOf(playedCard);
    Suit firstSuit = suitOf(game->trick[0]);
    Rank firstRank = rankOf(game->trick[0]);
    if (player->hand[cardIndex] == NULL_CARD) return RET_INPUT_ERROR;
    player->hand[cardIndex] = NULL_CARD;
    // Validation...v
    if (game->cardsPlayed == 0){
        if (playedCard != makeCard(suit_clubs, rank_2)) return RET_INPUT_ERROR; // first player must play 2 of clubs
        if (suitOf(playedCard) == suit_hearts || playedCard == makeCard(suit_spades, rank_Q)) return RET_INPUT_ERROR; // friendly trick; no hearts or queen of spades
    }
    if (!game->heartsDropped && game->cardsPlayed==0 && suitOf(playedCard) == suit_hearts) return RET_INPUT_ERROR; // no starting with hearts before hearts have been dropped
    if (game->cardsPlayed!=0){ // then we have to make sure the correct suit is being played
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
                    if (suitOf(player->hand[i]) == firstSuit) return RET_INPUT_ERROR; // tried to illegally claim void
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
    game->trick[game->turn] = playedCard;
    game->turn = (game->turn+1) % game->numPlayers;
    if (playedSuit == firstSuit && playedRank > firstRank) game->winner = game->turn;
    if (suitOf(playedCard) == suit_hearts || playedCard == makeCard(suit_spades, rank_Q)) game->heartsDropped = true;
    if (game->cardsPlayed >= game->numPlayers){ // last card has been played
        int score = 0;
        for (int i = 0; i<game->numPlayers; i++){
            if (suitOf(game->trick[i]) == suit_hearts) score++;
            else if (game->trick[i] == makeCard(suit_spades, rank_Q)) score += 13;
        }
        struct Player* winner = &game->players[game->winner];
        winner->score += score;
        game->turn = game->winner; // this player controls
    }
    return RET_NO_ERROR;
}

ReturnCode notifyPlayerOfMove(struct Player* player, int playerPlayed, card cardPlayed){ // fd is file descriptor, cardPlayed is the card that was played, playerPlayed is the player who just played that card
    sprintf(player->sendBuffer, "2%d,%d", playerPlayed, cardPlayed);
    size_t ret = cTalkSend(player->fd, player->sendBuffer, SEND_BUFFER_LEN);
    if (ret==0) return RET_NETWORK_ERROR;
    return RET_NO_ERROR;
}

int getMoveForPlayer(struct Player* player){
    int ret = cTalkSend(player->fd, "1", 2);
    if (ret==0) return RET_NETWORK_ERROR;
    ret = cTalkRecv(player->fd, player->recvBuffer, RECV_BUFFER_LEN);
    if (ret==0) return RET_NETWORK_ERROR;
    return atoi(player->recvBuffer);
}

ReturnCode runNewRound(struct Game* game){
    card deck[DECK_SIZE];
    int discarded = DECK_SIZE % game->numPlayers;
    game->deckSize = DECK_SIZE - discarded;
    game->trickNo = 0;
    
    // Shuffle deck...v
    bzero(deck, sizeof(card)*DECK_SIZE);
    int index;
    bool isUp;
    for (Suit suit = suit_start+1; suit < suit_end; suit++){
        for (Rank rank = rank_start+1; rank < rank_end; rank++){
            if (suit == suit_clubs && rank!=2 && rank-2 <= discarded) break;
            index = rand() % DECK_SIZE;
            if (deck[index]!=NULL_CARD){
                isUp = rand()%2;
                while (1){
                    index = (isUp?(index + 1):(index - 1)) % DECK_SIZE;
                    if (deck[index]==0) break;
                }
            }
            deck[index] = makeCard(suit, rank);
        }
    }
    //...^
    
    // Distribute cards...v
    int deckIndex = 0;
    struct Player* player;
    int cardsPerPlayer = DECK_SIZE / game->numPlayers;
    for (int playerIndex = 0; playerIndex < game->numPlayers; playerIndex++){
        player = &game->players[playerIndex];
        player->heartsVoid = false;
        player->clubsVoid = false;
        player->diamondsVoid = false;
        player->spadesVoid = false;
        for (int i = 0; i<cardsPerPlayer; i++){
            while (deck[deckIndex] == NULL_CARD) deckIndex++;
            player->hand[i] = deck[deckIndex];
            if (deck[deckIndex] == makeCard(suit_clubs, rank_2)) game->turn = playerIndex; // make the guy with the 2 of clubs go first
        }
    }
    //...^
    
    // Tell players that the game is starting...v
    char handBuffer[3];
    int addedLen;
    for (int playerIndex = 0; playerIndex < game->numPlayers; playerIndex++){
        player = &game->players[playerIndex];
        sprintf(player->sendBuffer, "3%d,%d,", game->numPlayers, game->turn);
        int bufIndex = strlen(player->sendBuffer);
        int cardsPerPlayer = game->deckSize / game->numPlayers;
        for (int handIndex = 0; handIndex < cardsPerPlayer; handIndex++){
            sprintf(handBuffer, "%d", player->hand[handIndex]);
            addedLen = strlen(handBuffer);
            memcpy(&player->sendBuffer[bufIndex], handBuffer, addedLen);
            bufIndex+=addedLen;
            player->sendBuffer[bufIndex] = ',';
            bufIndex++;
        }
        player->sendBuffer[bufIndex] = '\0';
        // By now, sendBuffer will be "3numPlayers,turn,hand1,hand2,hand3,...handN"
        int ret = cTalkSend(player->fd, player->sendBuffer, SEND_BUFFER_LEN); // notify each player that the game is starting, how many players are in, whose turn it is, and their hands
        if (ret==0) return RET_NETWORK_ERROR;
    }
    //...^
    
    //Play...v
    game->heartsDropped = false;
    game->winner = 0;
    while (game->trickNo < game->deckSize / game->numPlayers){
        memset(game->trick, NULL_CARD, MAX_PLAYERS*sizeof(card));
        game->cardsPlayed = 0;
        while (game->cardsPlayed < game->numPlayers){
            int play = getMoveForPlayer(&game->players[game->turn]);
            if (play==RET_NETWORK_ERROR) return RET_NETWORK_ERROR;
            handlePlay(game, play);
            for (int playerIndex = 0; playerIndex < game->numPlayers; playerIndex++){
                int ret = notifyPlayerOfMove(&game->players[playerIndex], game->turn, game->trick[game->cardsPlayed-1]);
                if (ret!=RET_NO_ERROR) return ret;
            }
        }
        game->trickNo++;
    }
    //...^
    
    return RET_NO_ERROR;
}

struct Game* createGame(int numPlayers, int* fds){
    if (numPlayers < 3 || numPlayers > 5) return NULL; // TODO error checking
    struct Game* game = emalloc(sizeof(struct Game));
    game->numPlayers = numPlayers;
    bzero(game->trick, MAX_PLAYERS);
    for (int i = 0; i < numPlayers; i++){
        struct Player player;
        player.fd = fds[i];
        game->players[i] = player;
    }
    return game;
}

ReturnCode runGames(int threadNo){
    for (int i = 0; i<threadNo; i++){
        
    }
}

int main(int argc, const char * argv[]) {
    
    return 0;
}



























