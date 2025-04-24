#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      
#include <arpa/inet.h>   
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"      
#define SERVER_PORT 12345       
#define BUFFER_SIZE 1024
#define MAX_CARDS 5

// Structure to represent a card
typedef struct {
    char name[20];
    int power;         // Attack or Defense value
    char type[10];     // "Attack" or "Defense"
} Card;

// Structure to represent a player
typedef struct {
    int health;
    Card hand[MAX_CARDS];
    int hand_size;
} Player;

// Structure to represent the game state
typedef struct {
    Player player;
    Player opponent;
    int your_turn;     // 1 if it's your turn, 0 otherwise
} GameState;

// Function prototypes
int  connect_to_server();
int  receive_full_message(int sockfd, char *buffer, size_t size);
int  send_full_message(int sockfd, const char *message);
void receive_game_state(int sockfd, GameState *state);
void parse_game_state(const char *msg, GameState *state);
void display_game_state(const GameState *state);
int  get_player_choice(const GameState *state);
void send_player_choice(int sockfd, int choice);
void trim_newline(char *str);

int main() {
    // Connect to the server
    int sockfd = connect_to_server();
    if (sockfd < 0) {
        fprintf(stderr, "Failed to connect to the server.\n");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server at %s:%d\n", SERVER_IP, SERVER_PORT);

    GameState game_state;
    memset(&game_state, 0, sizeof(GameState));

    // Main game loop
    while (1) {
        receive_game_state(sockfd, &game_state);
        display_game_state(&game_state);

        // Check for game over condition
        if (game_state.player.health <= 0) {
            printf("You have been defeated! Game Over.\n");
            break;
        } else if (game_state.opponent.health <= 0) {
            printf("Congratulations! You have won the game.\n");
            break;
        }

        // If it's the player's turn, prompt for action
        if (game_state.your_turn) {
            int choice = get_player_choice(&game_state);
            send_player_choice(sockfd, choice);
        } else {
            printf("Waiting for opponent's move...\n");
            sleep(1);
        }
    }
    close(sockfd);
    printf("Disconnected from server. Exiting.\n");
    return 0;
}

// Function to establish a TCP connection to the server
int connect_to_server() {
    int sockfd;
    struct sockaddr_in server_addr;

    // Create a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    // Define the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Convert IP address from text to binary
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        return -1;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Function to receive a complete message from the server
int receive_full_message(int sockfd, char *buffer, size_t size) {
    size_t total_received = 0;
    ssize_t bytes_received;

    while (total_received < size - 1) {
        bytes_received = recv(sockfd, buffer + total_received, size - 1 - total_received, 0);
        if (bytes_received < 0) {
            perror("recv");
            return -1;
        } else if (bytes_received == 0) {
            // Connection closed by server
            printf("Server disconnected.\n");
            return 0;  // Let caller handle
        }
        total_received += bytes_received;
        if (buffer[total_received - 1] == '\n') {
            break;
        }
    }

    buffer[total_received] = '\0';
    return total_received;
}

// Function to send a complete message to the server
int send_full_message(int sockfd, const char *message) {
    size_t total_sent = 0;
    size_t message_len = strlen(message);
    ssize_t bytes_sent;

    while (total_sent < message_len) {
        bytes_sent = send(sockfd, message + total_sent, message_len - total_sent, 0);
        if (bytes_sent < 0) {
            perror("send");
            return -1;
        }
        total_sent += bytes_sent;
    }

    return total_sent;
}

// Function to receive the game state from the server
void receive_game_state(int sockfd, GameState *state) {
    char buffer[BUFFER_SIZE];
    int bytes_received = receive_full_message(sockfd, buffer, sizeof(buffer));

    if (bytes_received < 0) {
        printf("Failed to receive data from server.\n");
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        exit(EXIT_SUCCESS);
    }
    trim_newline(buffer);
    parse_game_state(buffer, state);
}

void parse_game_state(const char *msg, GameState *state) {
    memset(state, 0, sizeof(*state));

    // 1) Find "YOUR_HEALTH:"
    {
        const char *key = "YOUR_HEALTH:";
        const char *found = strstr(msg, key);
        if (found) {
            found += strlen(key);
            state->player.health = atoi(found);
        }
    }

    // 2) Find "OPPONENT_HEALTH:"
    {
        const char *key = "OPPONENT_HEALTH:";
        const char *found = strstr(msg, key);
        if (found) {
            found += strlen(key);
            state->opponent.health = atoi(found);
        }
    }

    // 3) Find "YOUR_TURN:"
    {
        const char *key = "YOUR_TURN:";
        const char *found = strstr(msg, key);
        if (found) {
            found += strlen(key);
            state->your_turn = atoi(found);
        }
    }

    // 4) Find "CARDS:"
    {
        const char *key = "CARDS:";
        const char *found = strstr(msg, key);
        if (found) {
            found += strlen(key);

            char cards_buffer[512];
            memset(cards_buffer, 0, sizeof(cards_buffer));

            const char *sem_pos = strchr(found, ';');
            if (!sem_pos) {
                strncpy(cards_buffer, found, sizeof(cards_buffer) - 1);
            } else {
                size_t len = sem_pos - found;
                if (len >= sizeof(cards_buffer)) {
                    len = sizeof(cards_buffer) - 1;
                }
                strncpy(cards_buffer, found, len);
                cards_buffer[len] = '\0';
            }

            int card_count = 0;
            char *saveptr;
            char *token = strtok_r(cards_buffer, "|", &saveptr);
            while (token && card_count < MAX_CARDS) {
                char temp[64];
                strncpy(temp, token, sizeof(temp) - 1);
                temp[sizeof(temp) - 1] = '\0';

                // Parse each part
                char *inner_saveptr;
                char *name   = strtok_r(temp, ",", &inner_saveptr);
                char *type   = strtok_r(NULL, ",", &inner_saveptr);
                char *power  = strtok_r(NULL, ",", &inner_saveptr);

                if (name && type && power) {
                    strncpy(state->player.hand[card_count].name, name,
                            sizeof(state->player.hand[card_count].name) - 1);
                    state->player.hand[card_count].name[sizeof(state->player.hand[card_count].name) - 1] = '\0';

                    strncpy(state->player.hand[card_count].type, type,
                            sizeof(state->player.hand[card_count].type) - 1);
                    state->player.hand[card_count].type[sizeof(state->player.hand[card_count].type) - 1] = '\0';

                    state->player.hand[card_count].power = atoi(power);
                    card_count++;
                }
                token = strtok_r(NULL, "|", &saveptr);
            }

            state->player.hand_size = card_count;
        }
    }
}

// Display the current game state to the player
void display_game_state(const GameState *state) {
    printf("\n-----------------------------\n");
    printf("Your Health: %d\n", state->player.health);
    printf("Opponent's Health: %d\n\n", state->opponent.health);

    printf("Your Hand:\n");
    for (int i = 0; i < state->player.hand_size; i++) {
        printf("%d. %s (%s, Power: %d)\n",
               i + 1,
               state->player.hand[i].name,
               state->player.hand[i].type,
               state->player.hand[i].power);
    }
    printf("-----------------------------\n\n");
}

// Prompt the player to select a card to play
int get_player_choice(const GameState *state) {
    int choice;
    printf("It's your turn. Select a card to play (1-%d): ", state->player.hand_size);
    while (1) {
        if (scanf("%d", &choice) != 1) {
            // Clear invalid input
            while (getchar() != '\n');
            printf("Invalid input. Please enter a number between 1 and %d: ", state->player.hand_size);
            continue;
        }
        if (choice < 1 || choice > state->player.hand_size) {
            printf("Invalid choice. Please select a card number between 1 and %d: ", state->player.hand_size);
            continue;
        }
        break;
    }
    while (getchar() != '\n');
    return choice;
}

// Send the player's chosen card to the server
void send_player_choice(int sockfd, int choice) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "PLAY_CARD:%d\n", choice);

    if (send_full_message(sockfd, message) < 0) {
        printf("Failed to send your move to the server.\n");
        exit(EXIT_FAILURE);
    }
}

void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len == 0)
        return;
    if (str[len - 1] == '\n')
        str[len - 1] = '\0';
}
