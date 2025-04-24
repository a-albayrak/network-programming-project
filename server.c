#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>         
#include <arpa/inet.h>      
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>

#define SERVER_PORT 12345          
#define BUFFER_SIZE 1024
#define MAX_CARDS 5
#define MAX_PLAYERS 2

// Structure to represent a card
typedef struct {
    char name[20];
    int power;         // Attack or Defense value
    char type[10];     // "Attack" or "Defense"
} Card;

// Structure to represent a player
typedef struct {
    int sockfd;
    int health;
    Card hand[MAX_CARDS];
    int hand_size;
} Player;

// Structure to represent the game state
typedef struct {
    Player players[MAX_PLAYERS];
    int current_turn;    // Index of the player whose turn it is
    int game_over;
} GameState;

// Function prototypes
int  setup_server();
void accept_players(int server_sockfd, GameState *game_state, FILE *log_file);
void initialize_game(GameState *game_state);
void send_game_state(Player *player, GameState *game_state);
void handle_player_move(GameState *game_state, int player_index, const char *message, FILE *log_file);
void broadcast_game_state(GameState *game_state);
void remove_newline(char *str);

// Logging helper
void write_action_log(FILE *log_file, const char *format, ...);

int main() {
    // Open a log file in append mode
    FILE *log_file = fopen("game.log", "a");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }

    // Write a header to indicate server start time
    time_t t = time(NULL);
    fprintf(log_file, "=== Server started at %s", ctime(&t));
    fflush(log_file);

    // Initialize the game state
    GameState game_state;
    memset(&game_state, 0, sizeof(GameState));

    // Setup server
    int server_sockfd = setup_server();
    printf("Server is running on port %d. Waiting for players to connect...\n", SERVER_PORT);

    // Accept players
    accept_players(server_sockfd, &game_state, log_file);

    // Initialize game
    initialize_game(&game_state);
    printf("Both players connected. Starting the game...\n");
    write_action_log(log_file, "Both players connected. Starting the game.\n");

    // Broadcast initial game state
    broadcast_game_state(&game_state);

    // Main game loop
    while (!game_state.game_over) {
        int current_player = game_state.current_turn;
        Player *player = &game_state.players[current_player];
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));

        // Wait for the current player's move
        ssize_t bytes_received = recv(player->sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            // Player disconnected or error
            if (bytes_received == 0) {
                printf("Player %d disconnected. Ending game.\n", current_player + 1);
                write_action_log(log_file, "Player %d disconnected. Ending game.\n", current_player + 1);
            } else {
                perror("recv");
                write_action_log(log_file, "Error receiving from Player %d, ending game.\n", current_player + 1);
            }
            game_state.game_over = 1;
            break;
        }

        buffer[bytes_received] = '\0';
        remove_newline(buffer);

        printf("Received from Player %d: %s\n", current_player + 1, buffer);
        write_action_log(log_file, "Received from Player %d: %s\n", current_player + 1, buffer);

        // Handle the player's move
        handle_player_move(&game_state, current_player, buffer, log_file);

        // Check for win condition
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game_state.players[i].health <= 0) {
                printf("Player %d has been defeated!\n", i + 1);
                write_action_log(log_file, "Player %d has been defeated!\n", i + 1);
                game_state.game_over = 1;
                break;
            }
        }

        if (!game_state.game_over) {
            // Switch turn to the other player
            game_state.current_turn = (game_state.current_turn + 1) % MAX_PLAYERS;
            // Broadcast updated game state
            broadcast_game_state(&game_state);
        }
    }

    // Send final game state to both players
    broadcast_game_state(&game_state);

    // Close all sockets
    for (int i = 0; i < MAX_PLAYERS; i++) {
        close(game_state.players[i].sockfd);
    }
    close(server_sockfd);

    // Log server shutdown
    time_t end_time = time(NULL);
    fprintf(log_file, "=== Server shutting down at %s", ctime(&end_time));
    fflush(log_file);
    fclose(log_file);

    printf("Game has ended. Server shutting down.\n");
    return 0;
}

// Set up the server socket
int setup_server() {
    int sockfd;
    struct sockaddr_in server_addr;

    // Create a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Allow address reuse
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Define the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the socket to the address
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(sockfd, MAX_PLAYERS) < 0) {
        perror("Listen failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

// Accept player connections
void accept_players(int server_sockfd, GameState *game_state, FILE *log_file) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        int new_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if (new_sockfd < 0) {
            perror("Accept failed");
            i--; 
            continue;
        }
        game_state->players[i].sockfd = new_sockfd;
        game_state->players[i].health = 20;

        printf("Player %d connected from %s:%d\n", i + 1,
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        write_action_log(log_file, 
                         "Player %d connected from %s:%d\n", 
                         i + 1, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Initialize player's hand
        if (i == 0) { // Player 1
            strcpy(game_state->players[i].hand[0].name, "Fireball");
            strcpy(game_state->players[i].hand[0].type, "Attack");
            game_state->players[i].hand[0].power = 7;

            strcpy(game_state->players[i].hand[1].name, "Shield");
            strcpy(game_state->players[i].hand[1].type, "Defense");
            game_state->players[i].hand[1].power = 5;

            strcpy(game_state->players[i].hand[2].name, "Lightning Strike");
            strcpy(game_state->players[i].hand[2].type, "Attack");
            game_state->players[i].hand[2].power = 6;

            strcpy(game_state->players[i].hand[3].name, "Heal");
            strcpy(game_state->players[i].hand[3].type, "Defense");
            game_state->players[i].hand[3].power = 4;

            strcpy(game_state->players[i].hand[4].name, "Sword Slash");
            strcpy(game_state->players[i].hand[4].type, "Attack");
            game_state->players[i].hand[4].power = 5;

            game_state->players[i].hand_size = MAX_CARDS;
        } else { // Player 2
            strcpy(game_state->players[i].hand[0].name, "Ice Blast");
            strcpy(game_state->players[i].hand[0].type, "Attack");
            game_state->players[i].hand[0].power = 7;

            strcpy(game_state->players[i].hand[1].name, "Barrier");
            strcpy(game_state->players[i].hand[1].type, "Defense");
            game_state->players[i].hand[1].power = 5;

            strcpy(game_state->players[i].hand[2].name, "Earthquake");
            strcpy(game_state->players[i].hand[2].type, "Attack");
            game_state->players[i].hand[2].power = 6;

            strcpy(game_state->players[i].hand[3].name, "Rejuvenate");
            strcpy(game_state->players[i].hand[3].type, "Defense");
            game_state->players[i].hand[3].power = 4;

            strcpy(game_state->players[i].hand[4].name, "Axe Chop");
            strcpy(game_state->players[i].hand[4].type, "Attack");
            game_state->players[i].hand[4].power = 5;

            game_state->players[i].hand_size = MAX_CARDS;
        }
    }

    // Set the starting player (Player 1)
    game_state->current_turn = 0;
    game_state->game_over = 0;
}

// Initialize the game
void initialize_game(GameState *game_state) {
}

// Send the current game state to a specific player
void send_game_state(Player *player, GameState *game_state) {
    char message[BUFFER_SIZE];
    memset(message, 0, sizeof(message));

    // Calculate player index and opponent index
    int player_index   = (int)(player - game_state->players);  
    int opponent_index = 1 - player_index;                    

    // Construct the message
    snprintf(message, sizeof(message),
             "YOUR_HEALTH:%d;OPPONENT_HEALTH:%d;YOUR_TURN:%d;CARDS:",
             player->health,
             game_state->players[opponent_index].health,
             (game_state->current_turn == player_index) ? 1 : 0);

    // Append each card
    for (int i = 0; i < player->hand_size; i++) {
        char card_info[50];
        snprintf(card_info, sizeof(card_info), "%s,%s,%d",
                 player->hand[i].name,
                 player->hand[i].type,
                 player->hand[i].power);
        strcat(message, card_info);
        if (i < player->hand_size - 1) {
            strcat(message, "|");
        }
    }
    strcat(message, "\n");

    // Send the message to the player
    if (send(player->sockfd, message, strlen(message), 0) < 0) {
        perror("send");
    }
}

// Handle a player's move
void handle_player_move(GameState *game_state, int player_index, const char *message, FILE *log_file) {
    if (strncmp(message, "PLAY_CARD:", 10) != 0) {
        printf("Invalid message from Player %d: %s\n", player_index + 1, message);
        write_action_log(log_file, "Invalid message from Player %d: %s\n", player_index + 1, message);
        return;
    }

    int card_choice = atoi(message + 10);
    if (card_choice < 1 || card_choice > game_state->players[player_index].hand_size) {
        printf("Player %d selected an invalid card: %d\n", player_index + 1, card_choice);
        write_action_log(log_file, "Player %d selected an invalid card: %d\n", player_index + 1, card_choice);
        return;
    }

    // Retrieve the selected card
    Card *selected_card = &game_state->players[player_index].hand[card_choice - 1];
    printf("Player %d played %s (%s, Power: %d)\n",
           player_index + 1, selected_card->name, selected_card->type, selected_card->power);
    write_action_log(log_file, 
                     "Player %d played %s (%s, Power: %d)\n",
                     player_index + 1, selected_card->name, selected_card->type, selected_card->power);

    // Apply the card's effect to the opponent or self
    int opponent_index = 1 - player_index;
    if (strcmp(selected_card->type, "Attack") == 0) {
        game_state->players[opponent_index].health -= selected_card->power;
        if (game_state->players[opponent_index].health < 0) {
            game_state->players[opponent_index].health = 0;
        }
    } else if (strcmp(selected_card->type, "Defense") == 0) {
        game_state->players[player_index].health += selected_card->power;
        if (game_state->players[player_index].health > 20) { // Max health
            game_state->players[player_index].health = 20;
        }
    }
    selected_card->power -= 1;
}

// Broadcast the game state to all players
void broadcast_game_state(GameState *game_state) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        send_game_state(&game_state->players[i], game_state);
    }
}

void remove_newline(char *str) {
    size_t len = strlen(str);
    if (len == 0)
        return;
    if (str[len - 1] == '\n')
        str[len - 1] = '\0';
}

// Helper: Log action messages to file (similar to printf)
#include <stdarg.h>
void write_action_log(FILE *log_file, const char *format, ...) {
    if (!log_file) return;

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}
