# 421 Project Documentation  
**Author: Abdullah Albayrak**

---

## 1. Introduction

This project is a turn-based card game implemented in the **C programming language**, using **TCP sockets** for network communication.  
There is a **server** that hosts the game and manages the game logic, and one or more **clients** that connect to the server to play the game in real-time.

---

## 2. Project Structure

- `client.c`: Source code for the client program.  
- `server.c`: Source code for the server program.  
- `Makefile`: Used to compile both client and server.  
- `game.log` (auto-generated): Log file created and appended by the server at runtime.

---

## 3. What the Service Does

### Game Overview

This program creates a turn-based card duel:

1. Server waits for two players to connect.  
2. When both Client 1 and Client 2 are connected, the game starts.  
3. Each player has a selection of 5 cards, each with a type (**Attack** or **Defense**) and a power value.  
4. Players take turns selecting a card to play:  
   - **Attack Cards**: Reduce opponent’s health by the card’s power.  
   - **Defense Cards**: Increase the player’s own health by the card’s power (up to a maximum of 20).  
5. The game ends when a player’s health drops to 0 or below, or if someone disconnects.

### Action Logs

- The server writes every important action or event to `game.log`.  
- Events such as connections, disconnections, invalid inputs, and card plays are recorded.

### Disconnections

- If a client disconnects, the server detects this (`recv()` returns 0) and ends the game cleanly.  
- The client checks if the server closes the connection, then displays a message and exits.

---

## 4. Building & Running the Project

1. Open a terminal on your Linux system and navigate to the project directory.  
2. Compile using `make`  
3. This will produce two executables: `server`, `client`  
4. Run the Server: `./server`  
5. Run the Client (in another terminal or machine): `./client`  
6. Repeat step 4 for the second client in a separate terminal or separate machine.  
7. Once both clients are connected, **Game On!**  
   - Clients take turns selecting cards to attack or defend.  
   - Watch the server terminal and `game.log` for logs of actions.

---

## 5. How to Play

### Server Terminal

- Will log connection status and game progression.

### Client Terminal

- Displays your health and your opponent’s health.  
- Lists the cards in your hand, with Attack or Defense type and power value.  
- If it’s your turn, type the card number to play.  
- If it’s not your turn, wait for the opponent to finish.

**Winning Condition**: Reduce the opponent’s health to 0 or below.  
**Losing Condition**: Let your own health reach 0 or below.

---

## 6. Screenshots

1. **Server Startup**  
2. **Client 1 and Client 2 Connected**  
3. **Gameplay**  
   - Client 1’s Turn  
   - Client 2’s Turn  
   - When Game Ends  
4. **Action Log**

---

## 7. Challenges and Observations

- **TCP** was chosen for its reliable, connection-oriented communication, which is crucial for a game where each turn’s message (cards played, updated health) must arrive without corruption or loss.  
- **Blocking sockets** were used for straightforward turn-based gameplay:  
  - The client blocks on `recv()` for new state updates.  
  - The server blocks on `recv()` for a client’s move.  
- The server runs a **single game for two players**:  
  - No need for a thread per client or an event-driven model.  
  - Simply accept two connections and loop through receiving data and updating the game state in a single thread.

---

## References

1. *The Definitive Guide to Linux Network Programming* — Keir Davis, John W. Turner, and Nathan Yocom  
2. GeeksforGeeks - [Socket Programming in C/C++](https://www.geeksforgeeks.org/socket-programming-cc/)  
3. TutorialsPoint - [Socket Programming](https://www.tutorialspoint.com/unix_sockets/index.htm)

