#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <pthread.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define PORT 5656
#define MAX_CLIENTS 3
#define BUFFER_SIZE 1024

// Auction items
const char *items[] = {
    "Antique Vase",
    "Rare Painting",
    "Gold Necklace",
    "Vintage Clock",
    "Diamond Ring"
};
const int num_items = sizeof(items) / sizeof(items[0]);

int current_item_index = 0;
int highest_bid = 0;
char highest_bidder[50] = ""; // Stores highest bidder's name
int timer = 30;
SOCKET clients[MAX_CLIENTS] = {0};
char client_names[MAX_CLIENTS][50]; // Stores client names
pthread_mutex_t lock;

// Function to clear the console line for the visual timer
void clear_line() {
    printf("\r                                                      \r");
}

// Timer Thread: Updates the timer and displays it visually
void *timer_thread(void *arg) {
    while (1) {
        Sleep(1000); // Sleep for 1 second
        pthread_mutex_lock(&lock);
        timer--;

        clear_line();
        printf("Item: %s | Time Remaining: %02d seconds | Highest Bid: %d by %s",
               items[current_item_index], timer, highest_bid,
               (strlen(highest_bidder) > 0) ? highest_bidder : "No Bids");
        fflush(stdout);

        if (timer <= 0) {
            clear_line();
            printf("\n\nAuction ended for item: %s\n", items[current_item_index]);
            printf("Winner: %s with bid: %d\n\n",
                   (strlen(highest_bidder) > 0) ? highest_bidder : "No Winner",
                   highest_bid);

            // Notify all clients of auction end
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "Auction ended for item: %s\nWinner: %s with bid %d\n",
                     items[current_item_index],
                     (strlen(highest_bidder) > 0) ? highest_bidder : "No Winner",
                     highest_bid);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] != 0) {
                    send(clients[i], message, strlen(message), 0);
                }
            }

            // Move to the next item
            current_item_index = (current_item_index + 1) % num_items;
            highest_bid = 0;
            strcpy(highest_bidder, "");
            timer = 30; // Reset timer for the next item
            printf("\nNext item: %s\n", items[current_item_index]);
        }

        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// Handle individual client
void *handle_client(void *socket_desc) {
    SOCKET client_socket = *(SOCKET *)socket_desc;
    char buffer[BUFFER_SIZE];
    char client_name[50]; // Stores the client's name
    char client_ip[16];   // IPv4 address string length

    // Get client IP address
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    getpeername(client_socket, (struct sockaddr *)&addr, &addr_len);
    strcpy(client_ip, inet_ntoa(addr.sin_addr)); // Get client IP address

    // Receive the client's name
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    strncpy(client_name, buffer, sizeof(client_name) - 1);
    printf("\nClient connected: %s (%s)\n", client_name, client_ip);

    // Store the client's name
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client_socket) {
            strncpy(client_names[i], client_name, sizeof(client_names[i]) - 1);
            break;
        }
    }
    pthread_mutex_unlock(&lock);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("\nClient disconnected: %s\n", client_name);
            closesocket(client_socket);

            pthread_mutex_lock(&lock);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] == client_socket) {
                    clients[i] = 0;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);
            break;
        }

        int bid = atoi(buffer);
        pthread_mutex_lock(&lock);
        if (bid > highest_bid) {
            highest_bid = bid;
            strncpy(highest_bidder, client_name, sizeof(highest_bidder) - 1);
            timer = 10; // Reset timer
            printf("\nNew highest bid: %d by %s\n", highest_bid, highest_bidder);

            // Notify all clients
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "New highest bid: %d by %s\n", highest_bid, highest_bidder);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] != 0 && clients[i] != client_socket) {
                    send(clients[i], message, strlen(message), 0);
                }
            }
        } else {
            send(client_socket, "Bid too low!\n", 13, 0);
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// Start the server
void start_server() {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);
    pthread_t timer_tid;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Create server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) == SOCKET_ERROR) {
        printf("Listen failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Auction server started on port %d\n", PORT);
    printf("First item: %s\n", items[current_item_index]);

    // Initialize mutex
    pthread_mutex_init(&lock, NULL);

    // Start timer thread
    if (pthread_create(&timer_tid, NULL, timer_thread, NULL) != 0) {
        printf("Timer thread creation failed\n");
        closesocket(server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    int connected_clients = 0;
    while (connected_clients < MAX_CLIENTS) {
        // Accept new client
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) == INVALID_SOCKET) {
            printf("Accept failed. Error Code: %d\n", WSAGetLastError());
            continue;
        }

        // Store client
        pthread_mutex_lock(&lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == 0) {
                clients[i] = client_socket;
                break;
            }
        }
        pthread_mutex_unlock(&lock);

        connected_clients++;
        printf("\nNew client connected. Total connected: %d/%d\n", connected_clients, MAX_CLIENTS);

        // Create thread for client
        pthread_t client_tid;
        if (pthread_create(&client_tid, NULL, handle_client, (void *)&client_socket) != 0) {
            printf("Client thread creation failed\n");
            closesocket(client_socket);
        }
    }

    // Wait for all threads to finish
    pthread_join(timer_tid, NULL);
    pthread_mutex_destroy(&lock);
    closesocket(server_socket);
    WSACleanup();
}
