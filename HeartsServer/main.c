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
#include <pthread.h>
#include <fcntl.h>
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
#define isValidCard(card) (suitOf(card) > suit_start && suitOf(card) < suit_end && rankOf(card) > rank_start && rankOf(card) < rank_end)
#define DECK_SIZE (52)
#define MIN_PLAYERS (3)
#define MAX_PLAYERS (6)
#define MAX_CARDS_PER_PLAYER (DECK_SIZE/MIN_PLAYERS)
typedef int card; // card value = rank*RANK_PRIME + suit*SUIT_PRIME
// Communcation constants ===vv
#define MAX_NAME_LENGTH (16)
#define SEND_BUFFER_LEN (10 + MAX_CARDS_PER_PLAYER*3)
#define RECV_BUFFER_LEN (4 + MAX_NAME_LENGTH)
#define DEBUG_PRINT if (0) printf
// ===^^
struct Player{
    char* name; // ASCII name of player (example: "Alec_Z_Bot")
    int in; // file input descriptor for AI comm
    int out; //file output descriptor for AI comm
    //char sendBuffer[SEND_BUFFER_LEN];
    //char recvBuffer[RECV_BUFFER_LEN];
    int newPoints; // The points added onto the score last round (negated if the player shoots the moon).
    uint64_t score; // The player's score
    card* hand; // The player's hand.
    bool canShootMoon; // Starts as true at beginning of round. If this is true by the end of the round, the player has shot the moon.
    bool heartsVoid; // Is this guy void on... hearts (for optimization purposes)?
    bool clubsVoid; // ... clubs?
    bool diamondsVoid; // ... diamonds?
    bool spadesVoid; /// ... spades?
};
struct Game{
    char* sendBuffer; // The send buffer to be used for this game.
    char* recvBuffer; // The recv buffer to be used for this game.
    struct Player** players; // Players
    int numPlayers; // [MIN_PLAYERS, MAX_PLAYERS]
    int deckSize; // How many cards in deck?
    int firstPlayer; // Who went first?
    int turn; // Whose turn is it? This is an index in players.
    int winner; // Player who is currently poised to take the trick.
    card trick[MAX_PLAYERS]; // cards on table
    int cardsPlayed; // Number of cards played this trick
    int trickNo; // The trick number (starts at 0)
    bool heartsDropped; // Have hearts been dropped?
};
// ===^^

char* nameOfSuit(Suit suit){
    if (suit==suit_clubs) return "clubs";
    if (suit==suit_hearts) return "hearts";
    if (suit==suit_spades) return "spades";
    if (suit==suit_diamonds) return "diamonds";
    if (suit==suit_start) return "START_ERROR";
    return "END_ERROR";
}

ReturnCode handlePlay(struct Game* game, int cardIndex){
    struct Player* player = game->players[game->turn];
    card playedCard = player->hand[cardIndex];
    Suit playedSuit = suitOf(playedCard);
    Rank playedRank = rankOf(playedCard);
    Suit firstSuit = suitOf(game->trick[game->firstPlayer]);
    Rank highestRank = rankOf(game->trick[game->winner]);
    int cardsPerPlayer = (DECK_SIZE/game->numPlayers);
    DEBUG_PRINT("%s is playing the %d of %s.\n", player->name, playedRank+1, nameOfSuit(playedSuit));
    if (!isValidCard(playedCard)){
        return RET_INPUT_ERROR;
    }
    player->hand[cardIndex] = NULL_CARD;
    // Validation...v
    if (game->cardsPlayed == 0 && game->trickNo == 0){
        if (playedCard != makeCard(suit_clubs, rank_2)){
            return RET_INPUT_ERROR; // first player must play 2 of clubs
        }
    }
    if (!game->heartsDropped && game->cardsPlayed==0 && suitOf(playedCard) == suit_hearts){
        // We check to make sure the player has nothing but hearts...
        if (!(player->clubsVoid && player->diamondsVoid && player->spadesVoid)){
            player->spadesVoid = true;
            player->clubsVoid = true;
            player->diamondsVoid = true;
            card checkCard;
            for (int i = 0; i<cardsPerPlayer; i++){
                Suit suit = suitOf(player->hand[i]);
                switch(suit){
                    case suit_spades:{ player->spadesVoid = false; break; }
                    case suit_diamonds:{ player->diamondsVoid = false; break; }
                    case suit_clubs:{ player->clubsVoid = false; break; }
                }
            }
            if (!(player->clubsVoid && player->diamondsVoid && player->spadesVoid)){
                return RET_INPUT_ERROR; // Player isn't void on everything but hearts, but he tried to lead with hearts before they were broken.
            }
        }
    }
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
                for (int i = 0 ; i<cardsPerPlayer; i++){
                    if (suitOf(player->hand[i]) == firstSuit){
                        return RET_INPUT_ERROR; // tried to illegally claim void
                    }
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
    if (playedSuit == firstSuit && playedRank > highestRank) game->winner = game->turn;
    //else printf("playedSuit: %d, firstSuit: %d, playedRank: %d, highestRank: %d\n", playedSuit, firstSuit, playedRank, highestRank);
    if (suitOf(playedCard) == suit_hearts || playedCard == makeCard(suit_spades, rank_Q)) game->heartsDropped = true;
    game->cardsPlayed++;
    game->trick[game->turn] = playedCard;
    game->turn = (game->turn+1) % game->numPlayers;
    if (game->cardsPlayed >= game->numPlayers){ // last card has been played
        int score = 0;
        for (int i = 0; i<game->numPlayers; i++){ // Tally up points gained
            if (suitOf(game->trick[i]) == suit_hearts) score++;
            else if (game->trick[i] == makeCard(suit_spades, rank_Q)) score += 13;
        }
        if (score!=0){ // Nobody else can shoot the moon if points have been racked up
            for (int i = 0; i<game->numPlayers; i++){
                if (i != game->winner) game->players[i]->canShootMoon = false;
            }
        }
        if (game->trickNo != 0){
            game->players[game->winner]->newPoints = score; // Only add to score if it's not the first trick of the round
        }
        else game->players[game->winner]->newPoints = 0;
        game->turn = game->winner; // this player controls
        game->firstPlayer = game->turn;
        DEBUG_PRINT("%s won the trick for %d points.\n", game->players[game->winner]->name, game->players[game->winner]->newPoints);
    }
    return RET_NO_ERROR;
}

ReturnCode notifyPlayerOfMove(struct Player* player, int playerPlayed, card cardPlayed, char* sendBuffer){ // cardPlayed is the card that was played, playerPlayed is the player who just played that card
    sprintf(sendBuffer, "]%d,%d", playerPlayed, cardPlayed);
    size_t ret = cTalkSend(player->out, sendBuffer, SEND_BUFFER_LEN);
    if (ret==0) return RET_NETWORK_ERROR;
    return RET_NO_ERROR;
}

int getMoveForPlayer(struct Player* player, char* sendBuffer, char* recvBuffer){
    int ret = cTalkSend(player->out, "[", 2);
    if (ret==0){
        return RET_NETWORK_ERROR;
    }
    ret = cTalkRecv(player->in, recvBuffer, RECV_BUFFER_LEN);
    if (ret==0){
        return RET_NETWORK_ERROR;
    }
    return atoi(recvBuffer);
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
    unsigned int seed = (unsigned int)clock();
    srand(seed);
    //printf("Seed: %d\n", seed);
    for (Suit suit = suit_start+1; suit < suit_end; suit++){
        for (Rank rank = rank_start+1; rank < rank_end; rank++){
            if (suit == suit_clubs && rank!=2 && rank-2 <= discarded){
                break;
            }
            index = rand() % DECK_SIZE;
            if (deck[index]!=NULL_CARD){
                isUp = rand()%2;
                while (1){
                    if (isUp){
                        index++;
                        if (index >= DECK_SIZE) index-=DECK_SIZE;
                    }
                    else{
                        index--;
                        if (index < 0) index+=DECK_SIZE;
                    }
                    if (deck[index]==NULL_CARD) break;
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
        player = game->players[playerIndex];
        player->canShootMoon = true;
        player->heartsVoid = false;
        player->clubsVoid = false;
        player->diamondsVoid = false;
        player->spadesVoid = false;
        DEBUG_PRINT("Cards for %s: ", game->players[playerIndex]->name);
        for (int i = 0; i<cardsPerPlayer; i++){
            while (deck[deckIndex] == NULL_CARD){
                deckIndex++;
            }
            player->hand[i] = deck[deckIndex];
            DEBUG_PRINT("%d of %s, ", rankOf(deck[deckIndex])+1, nameOfSuit(suitOf(deck[deckIndex])));
            if (deck[deckIndex] == makeCard(suit_clubs, rank_2)) game->turn = playerIndex; // make the guy with the 2 of clubs go first
            deckIndex++;
        }
        DEBUG_PRINT("\n");
    }
    //...^
    
    // Tell players that the game is starting...v
    char handBuffer[3];
    int addedLen;
    game->firstPlayer = game->turn;
    for (int playerIndex = 0; playerIndex < game->numPlayers; playerIndex++){
        player = game->players[playerIndex];
        sprintf(game->sendBuffer, ":%d,%d,%d,", game->numPlayers, playerIndex, game->firstPlayer);
        int bufIndex = strlen(game->sendBuffer);
        int cardsPerPlayer = game->deckSize / game->numPlayers;
        for (int handIndex = 0; handIndex < cardsPerPlayer; handIndex++){
            sprintf(handBuffer, "%d", player->hand[handIndex]);
            addedLen = strlen(handBuffer);
            memcpy(&game->sendBuffer[bufIndex], handBuffer, addedLen);
            bufIndex+=addedLen;
            game->sendBuffer[bufIndex] = ',';
            bufIndex++;
        }
        bufIndex--;
        game->sendBuffer[bufIndex] = '\0';
        // By now, sendBuffer will be ":numPlayers,playerID,turn,hand1,hand2,hand3,...handN"
        int ret = cTalkSend(player->out, game->sendBuffer, strlen(game->sendBuffer)+1); // notify each player that the game is starting, how many players are in, the player's ID number [0, numPlayers-1], whose turn it is, and their hands
        if (ret==0){
            return RET_NETWORK_ERROR;
        }
    }
    //...^
    
    // Play...v
    game->heartsDropped = false;
    game->winner = game->firstPlayer;
    int lastTurn;
    int numTricks = game->deckSize / game->numPlayers;
    DEBUG_PRINT("Starting round with %d tricks.\n", numTricks);
    while (game->trickNo+1 < numTricks){
        memset(game->trick, NULL_CARD, MAX_PLAYERS*sizeof(card));
        game->cardsPlayed = 0;
        while (game->cardsPlayed < game->numPlayers){
            int play = getMoveForPlayer(game->players[game->turn], game->sendBuffer, game->recvBuffer);
            if (play==RET_NETWORK_ERROR){
                return RET_NETWORK_ERROR;
            }
            lastTurn = game->turn;
            if (handlePlay(game, play) != RET_NO_ERROR){
                return RET_INPUT_ERROR;
            }
            for (int playerIndex = 0; playerIndex < game->numPlayers; playerIndex++){
                if (playerIndex != lastTurn){
                    int ret = notifyPlayerOfMove(game->players[playerIndex], lastTurn, game->trick[lastTurn], game->sendBuffer);
                    if (ret!=RET_NO_ERROR){
                        return ret;
                    }
                }
            }
        }
        game->trickNo++;
        game->cardsPlayed = 0;
    }
    //...^
    
    // Add points...v
    int moonShot = -1;
    for (int i = 0; i<game->numPlayers; i++){
        if (game->players[i]->canShootMoon == true){
            if (moonShot != -1){
                printf("*** ERROR: MOON SHOT TWICE ***\n"); // TODO remove debug check
            }
            moonShot = i;
            printf("%s shot the moon!\n", game->players[i]->name);
            // break; // TODO uncomment to remove debug check
        }
    }
    if (moonShot != -1){
        for (int i = 0; i<game->numPlayers; i++){
            if (i != moonShot) game->players[i]->score = game->players[i]->score + 26;
        }
    }
    else{
        for (int i = 0; i<game->numPlayers; i++){
            game->players[i]->score = game->players[i]->score + game->players[i]->newPoints;
            //printf("%s added %d points.\n", game->players[i]->name, game->players[i]->newPoints);
        }
    }
    //...^
    
    return RET_NO_ERROR;
}

struct Game* createGame(int numPlayers, int* ins, int* outs, char* sendBuffer, char* recvBuffer){
    if (numPlayers < MIN_PLAYERS || numPlayers > MAX_PLAYERS) return NULL; // TODO error checking
    struct Game* game = emalloc(sizeof(struct Game));
    game->sendBuffer = sendBuffer;
    game->recvBuffer = recvBuffer;
    game->numPlayers = numPlayers;
    game->players = emalloc(sizeof(struct Player*)*game->numPlayers);
    bzero(game->trick, MAX_PLAYERS);
    for (int i = 0; i < numPlayers; i++){
        struct Player* player = emalloc(sizeof(struct Player));
        player->in = ins[i];
        player->out = outs[i];
        player->hand = emalloc(sizeof(card)*(DECK_SIZE/numPlayers));
        memset(player->hand, NULL_CARD, sizeof(card)*(DECK_SIZE/numPlayers));
        game->players[i] = player;
        int ret = cTalkSend(player->out, "@", 2); // Ask for the player's name
        if (ret==0){ printf("**Fatal error when asking for name.**\n"); return NULL; } // error
        ret = cTalkRecv(player->in, game->recvBuffer, RECV_BUFFER_LEN); // Get the player's name in response
        if (ret==0){ printf("**Fatal error when receiving name.**\n"); return NULL; } // error
        player->name = strdup(game->recvBuffer);
        printf("√ Player %s joined game with in fd %d and out fd %d.\n", player->name, player->in, player->out);
    }
    return game;
}

void freeGame(struct Game* game){
    for (int i = 0; i<game->numPlayers; i++){
        efree(game->players[i]->name);
        efree(game->players[i]->hand);
        efree(game->players[i]);
    }
    efree(game->sendBuffer);
    efree(game->recvBuffer);
    efree(game);
}

struct ThreadArg{
    int threadNo; // Thread number
    int numPlayers; // Number of players
    uint64_t numTests; // Number of tests (game rounds) to run
    int* ins; // Input fds
    int* outs; // Output fds
    bool* start; // Whether or not to start the tests
};

void *gameThread(void *arg){ // One thread is used to one test one group.
    struct ThreadArg* threadArg = arg;
    while (!*(threadArg->start)){
        usleep(5000);
    }
    char* sendBuffer = emalloc(SEND_BUFFER_LEN);
    char* recvBuffer = emalloc(RECV_BUFFER_LEN);
    struct Game* game = createGame(threadArg->numPlayers, threadArg->ins, threadArg->outs, sendBuffer, recvBuffer);
    if (game==NULL){
        printf("***GAME THREAD %d IS STOPPING DUE TO FATAL ERROR CREATING GAME***\n", threadArg->threadNo);
    }
    ReturnCode ret;
    printf("Thread %d running tests...\n", threadArg->threadNo);
    for (uint64_t i = 0; i<threadArg->numTests; i++){
        ret = runNewRound(game);
        if (ret!=RET_NO_ERROR){
            printf("***GAME THREAD %d IS STOPPING DUE TO ERROR: ERROR CODE %d***\n", threadArg->threadNo, ret);
            break;
        }
    }
    printf("\n=== Thread %d finished ===\n========================v\n", threadArg->threadNo);
    for (int i = 0; i<threadArg->numPlayers; i++){
        printf("%s scored %llu.\n", game->players[i]->name, game->players[i]->score);
    }
    printf("========================^\n\n");
    for (int i = 0; i<threadArg->numPlayers; i++){
        cTalkSend(threadArg->outs[i], ";", 2);
    }
    freeGame(game);
    for (int i = 0; i<threadArg->numPlayers; i++){
        close(threadArg->ins[i]);
        close(threadArg->outs[i]);
    }
    efree(threadArg->ins);
    
    efree(threadArg->outs);
    efree(threadArg);
    pthread_detach(pthread_self());
    return NULL;
}

#define N00B_ALERT "Usage: HeartsServer [total number of players] [players per group] [number of times to test each group] [out1, in1, out2, in2, out3, in3, ..., ...]\n"

int main(int argc, const char * argv[]) {
    //Args...v
    /*
     0. Program name (unused)
     1. Total number of players [MIN_PLAYERS, infinity)
     2. Number of players per group (must be a factor of total number of players) [MIN_PLAYERS, MAX_PLAYERS]
     3. Number of times to test each group
     [4, numPlayers+4] (even). Output stream for each player
     [5, numPlayers+5] (odd). Input stream for each player
     */
    //...^
    // Take and validate args...V
    logLevel = LOG_ERROR;
    if (argc<3){
        printf(N00B_ALERT);
        exit(0);
    }
    int numPlayers = atoi(argv[1]);
    if (numPlayers < MIN_PLAYERS){
        printf("Invalid number of players specified; must be at least %d.\n", MIN_PLAYERS);
        printf(N00B_ALERT);
        exit(0);
    }
    int playersPerGroup = atoi(argv[2]);
    if (playersPerGroup < MIN_PLAYERS || playersPerGroup > MAX_PLAYERS){
        printf("Invalid players per group specified; must be within [%d, %d].\n", MIN_PLAYERS, MAX_PLAYERS);
        printf(N00B_ALERT);
        exit(0);
    }
    uint64_t numTests = atoll(argv[3]);
    if (numTests < 1){
        printf("Invalid number of tests specified; must be at least 1.\n");
        printf(N00B_ALERT);
        exit(0);
    }
    if (numPlayers*2 != (argc - 4)){
        printf("You need to supply exactly one output and one input stream per player.\n");
        printf(N00B_ALERT);
        exit(0);
    }
    if (numPlayers % playersPerGroup != 0){
        printf("The number of players per group must be a factor of the total number of players.\n");
        printf(N00B_ALERT);
        exit(0);
    }
    //...^
    // Make and run threads...v
    printf("Starting Hearts server with %d players, %d players per group, and %llu tests.\n", numPlayers, playersPerGroup, numTests);
    int numThreads = numPlayers/playersPerGroup;
    int fdIndex = 4;
    bool isRunningTests = false;
    for (int i = 0; i < numThreads; i++){
        pthread_t thread;
        struct ThreadArg* arg = emalloc(sizeof(struct ThreadArg));
        int* outs = emalloc(sizeof(int)*playersPerGroup);
        int* ins = emalloc(sizeof(int)*playersPerGroup);
        for (int i2 = 0; i2 < playersPerGroup*2; i2+=2){
            outs[i2/2] = open(argv[i2+fdIndex], O_WRONLY);
            printf("Opened FIFO for writing: %s\n", argv[i2+fdIndex]);
            ins[i2/2] = open(argv[i2+fdIndex+1], O_RDONLY);
            printf("Opened FIFO for reading: %s\n", argv[i2+fdIndex+1]);
            time_t start, stop;
            /*
            unsigned char pingBuffer[2];
            time(&start);
            cTalkSend(outs[i2/2], "^", 2);
            cTalkRecv(ins[i2/2], pingBuffer, 2);
            time(&stop);
            printf("Ping for player %d: %f seconds.\n", i2/2, difftime(stop, start));
             */
        }
        fdIndex+=2*playersPerGroup;
        arg->threadNo = i;
        arg->numPlayers = playersPerGroup;
        arg->numTests = numTests;
        arg->outs = outs;
        arg->ins = ins;
        arg->start = &isRunningTests;
        if (pthread_create(&thread, NULL, gameThread, (void*) arg) != 0){
            if (logLevel >= LOG_ERROR) printf("***FATAL ERROR: UNABLE TO CREATE A THREAD, RAGEQUITTING***\n");
            exit(0);
        }
        printf("Created thread %d.\n", i);
    }
    printf("All pipes connected and ready. Press RETURN to start the tests... ");
    char keyboardBuf[2];
    fgets(keyboardBuf, 2, stdin);
    isRunningTests = true;
    pause();
    //...^
    return 0;
}












