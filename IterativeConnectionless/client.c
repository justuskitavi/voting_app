/* =========================================================
 * client_udp.c  —  CONNECTIONLESS client for voting server
 *
 * CONNECTIONLESS aspects (look for the ★ markers):
 *  ★ SOCK_DGRAM   – UDP socket
 *  ★ connect()    – optional "pseudo-connect" that sets a default
 *                   destination so we can use send()/recv() instead
 *                   of sendto()/recvfrom(). The server is still UDP;
 *                   no TCP handshake occurs. This just avoids passing
 *                   the address on every call.
 *  ★ No guarantee of delivery or ordering (UDP semantics)
 *  ★ The client sends a CLI_INPUT datagram to start; the server
 *    responds with SVR_DISPLAY or SVR_PROMPT datagrams.
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT        9090
#define MAX_PAYLOAD 2048

typedef enum { SVR_DISPLAY = 1, SVR_PROMPT = 2, CLI_INPUT = 3 } MsgType;

/* Request and Response Types (new client-driven model) */
typedef enum {
    REQ_REGISTER_VOTER = 10,
    REQ_VERIFY_ADMIN = 20,
    REQ_CAST_VOTE = 30
} RequestType;

typedef enum {
    RESP_SUCCESS = 1,
    RESP_ERROR = 2,
    RESP_DATA = 3
} ResponseType;

/* Extended message structure to support both old and new protocols */
typedef struct {
    int  type;
    int  req_type;       /* For CLIENT_REQUEST messages */
    int  resp_status;    /* For SVR_RESPONSE: RESP_SUCCESS/RESP_ERROR/RESP_DATA */
    char text[MAX_PAYLOAD];
    
    /* Request payload fields (for client-driven requests) */
    struct {
        char name[50];
        char regNo[20];
        char password[20];
    } voter_data;
} Msg;

int main(void)
{
    /* Prompt the user for the server IP at runtime */
    char server_ip[64];
    printf("Enter server IP address: ");
    fflush(stdout);
    if (fgets(server_ip, sizeof(server_ip), stdin) == NULL) {
        fprintf(stderr, "Failed to read IP address.\n");
        exit(1);
    }
    server_ip[strcspn(server_ip, "\r\n")] = '\0';  /* strip newline */

    /* ★ CONNECTIONLESS: SOCK_DGRAM = UDP */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", server_ip);
        exit(1);
    }

    /* ★ connect() on a UDP socket does NOT perform a TCP handshake.
     *   It simply stores the server address so that subsequent send()
     *   and recv() calls use it automatically. The "connection" exists
     *   only inside the OS socket struct; no packet is transmitted. */
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        fprintf(stderr, "Make sure the UDP server is running on %s:%d\n",
                server_ip, PORT);
        exit(1);
    }

    printf("[udp-client] Using connectionless UDP to %s:%d\n", server_ip, PORT);

    /* Send an initial connection message */
    {
        Msg init;
        memset(&init, 0, sizeof(init));
        init.type = 99;  /* Response message type */
        send(sock, &init, sizeof(init), 0);
    }

    /* ============================================================ */
    /*  CLIENT-SIDE STATE MACHINE: Handle all prompting locally     */
    /* ============================================================ */
    
    printf("\n[UDP Voting Client - Client-Driven UI]\n");
    printf("All prompts are now local to this client.\n");
    printf("Server only receives requests and sends responses.\n\n");
    
    int running = 1;
    while (running) {
        /* Display main menu locally (NOT from server) */
        printf("\n=== Main Menu ===\n");
        printf("1. Admin Panel\n");
        printf("2. Register Voter\n");
        printf("3. Exit\n");
        printf("Enter choice: ");
        fflush(stdout);
        
        char choice[20];
        if (fgets(choice, sizeof(choice), stdin) == NULL) {
            printf("Error reading input.\n");
            break;
        }
        choice[strcspn(choice, "\n")] = '\0';
        
        if (strcmp(choice, "1") == 0) {
            /* ---- ADMIN PANEL (client-side input collection) ---- */
            printf("\n=== Admin Panel ===\n");
            char regNo[20], password[20];
            
            printf("Admin Reg No: ");
            fflush(stdout);
            if (fgets(regNo, sizeof(regNo), stdin) == NULL) continue;
            regNo[strcspn(regNo, "\n")] = '\0';
            
            printf("Admin Password: ");
            fflush(stdout);
            if (fgets(password, sizeof(password), stdin) == NULL) continue;
            password[strcspn(password, "\n")] = '\0';
            
            /* Validate locally */
            if (strlen(regNo) == 0 || strlen(password) == 0) {
                printf("Error: Empty fields. Please try again.\n");
                continue;
            }
            
            /* Build and send request */
            Msg req;
            memset(&req, 0, sizeof(req));
            req.type = 99;  /* Custom request type */
            req.req_type = REQ_VERIFY_ADMIN;
            strncpy(req.voter_data.regNo, regNo, sizeof(req.voter_data.regNo) - 1);
            strncpy(req.voter_data.password, password, sizeof(req.voter_data.password) - 1);
            
            send(sock, &req, sizeof(req), 0);
            
            /* Receive response */
            Msg resp;
            memset(&resp, 0, sizeof(resp));
            if (recv(sock, &resp, sizeof(resp), 0) > 0) {
                printf("\nServer Response: %s\n", resp.text);
                if (resp.resp_status == RESP_SUCCESS) {
                    printf("Admin authentication successful.\n");
                    printf("(Full admin panel not implemented in this demo.)\n");
                }
            } else {
                printf("No response from server.\n");
            }
            
        } else if (strcmp(choice, "2") == 0) {
            /* ---- VOTER REGISTRATION (client-side input collection) ---- */
            printf("\n=== Voter Registration ===\n");
            char name[50], regNo[20], password[20];
            
            printf("Voter name: ");
            fflush(stdout);
            if (fgets(name, sizeof(name), stdin) == NULL) continue;
            name[strcspn(name, "\n")] = '\0';
            
            printf("Registration number: ");
            fflush(stdout);
            if (fgets(regNo, sizeof(regNo), stdin) == NULL) continue;
            regNo[strcspn(regNo, "\n")] = '\0';
            
            printf("Password: ");
            fflush(stdout);
            if (fgets(password, sizeof(password), stdin) == NULL) continue;
            password[strcspn(password, "\n")] = '\0';
            
            /* Validate locally (client-side validation) */
            if (strlen(name) == 0 || strlen(regNo) == 0 || strlen(password) == 0) {
                printf("Error: Empty fields. Please try again.\n");
                continue;
            }
            if (strlen(password) < 4) {
                printf("Error: Password must be at least 4 characters.\n");
                continue;
            }
            
            /* Build and send request with all data at once */
            Msg req;
            memset(&req, 0, sizeof(req));
            req.type = 99;  /* Custom request type */
            req.req_type = REQ_REGISTER_VOTER;
            strncpy(req.voter_data.name, name, sizeof(req.voter_data.name) - 1);
            strncpy(req.voter_data.regNo, regNo, sizeof(req.voter_data.regNo) - 1);
            strncpy(req.voter_data.password, password, sizeof(req.voter_data.password) - 1);
            
            send(sock, &req, sizeof(req), 0);
            
            /* Receive response */
            Msg resp;
            memset(&resp, 0, sizeof(resp));
            if (recv(sock, &resp, sizeof(resp), 0) > 0) {
                printf("\nServer Response: %s\n", resp.text);
                if (resp.resp_status == RESP_ERROR) {
                    printf("(Registration may have failed. Check with server.)\n");
                }
            } else {
                printf("No response from server.\n");
            }
            
        } else if (strcmp(choice, "3") == 0) {
            printf("Goodbye!\n");
            running = 0;
        } else {
            printf("Invalid choice. Please try again.\n");
        }
    }

    close(sock);
    return 0;
}