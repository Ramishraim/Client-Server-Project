#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define SERVER_IP "127.0.0.1" // Replace with server's IP if not localhost
#define PORT 8080
#define BUFFER_SIZE 1024

void start_client() {
    WSADATA wsa;
    SOCKET client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char name[50]; // To store the client's name

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return;
    }

    // Create client socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection to server failed. Error Code: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    printf("Connected to the auction server at %s:%d\n", SERVER_IP, PORT);

    // Ask the client for their name
    printf("Enter your name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0'; // Remove newline character from the input

    // Send the name to the server
    if (send(client_socket, name, strlen(name), 0) < 0) {
        printf("Failed to send name. Error Code: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    printf("Welcome to the auction, %s! Start bidding!\n", name);

    // Client interaction loop
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        printf("Enter your bid (or type 'exit' to quit): ");
        fgets(buffer, BUFFER_SIZE, stdin);

        // Exit the program if the user types "exit"
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Exiting the auction.\n");
            break;
        }

        // Send bid to the server
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            printf("Failed to send bid. Error Code: %d\n", WSAGetLastError());
            break;
        }

        // Receive server response
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Server disconnected.\n");
            break;
        }
        printf("Server: %s\n", buffer);
    }

    // Clean up
    closesocket(client_socket);
    WSACleanup();
}
