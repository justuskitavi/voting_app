/* =========================================================
 * client_tcp.c  —  CONNECTION-ORIENTED client for voting server
 *
 * CONNECTION-ORIENTED aspects (look for the ★ markers):
 *  ★ SOCK_STREAM  – TCP socket; guarantees ordered, reliable delivery
 *  ★ connect()    – triggers the TCP three-way handshake (SYN /
 *                   SYN-ACK / ACK). After this returns, a real
 *                   persistent connection exists between client and server.
 *  ★ recv()/send() – stream I/O over the established connection
 *  ★ close()      – sends TCP FIN, initiating graceful teardown
 *
 * The client is essentially unchanged from the original — the
 * connection-oriented work is almost entirely on the socket type
 * and the connect() call. The stream abstraction hides the rest.
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

typedef struct {
    int  type;
    char text[MAX_PAYLOAD];
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

    /* ★ CONNECTION-ORIENTED: SOCK_STREAM = TCP */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", server_ip);
        exit(1);
    }

    /* ★ connect() — performs the TCP three-way handshake.
     *   SYN  →  (client to server)
     *   SYN-ACK ←  (server to client)
     *   ACK  →  (client to server)
     *   After this call succeeds, a reliable ordered stream is established.
     *   The server's accept() unblocks and returns a dedicated socket. */
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        fprintf(stderr, "Make sure the TCP server is running on %s:%d\n",
                server_ip, PORT);
        exit(1);
    }

    printf("[tcp-client] Connected (TCP) to %s:%d\n", server_ip, PORT);

    Msg  msg;
    char input[MAX_PAYLOAD];

    while (1) {
        /* ★ recv() — reads from the established TCP stream */
        int n = recv(sock, &msg, sizeof(msg), 0);
        if (n <= 0) {
            printf("\n[tcp-client] Server disconnected.\n");
            break;
        }

        switch (msg.type) {

        case SVR_DISPLAY:
            printf("%s", msg.text);
            fflush(stdout);
            break;

        case SVR_PROMPT:
            printf("%s", msg.text);
            fflush(stdout);

            if (fgets(input, sizeof(input), stdin) == NULL) {
                input[0] = '\n'; input[1] = '\0';
            }

            {
                Msg reply;
                memset(&reply, 0, sizeof(reply));
                reply.type = CLI_INPUT;
                strncpy(reply.text, input, MAX_PAYLOAD - 1);
                /* ★ send() — writes into the TCP stream; delivery is
                 *   guaranteed and ordered by the TCP protocol. */
                send(sock, &reply, sizeof(reply), 0);
            }
            break;

        default:
            break;
        }
    }

    /* ★ close() — sends TCP FIN, initiates graceful connection teardown */
    close(sock);
    return 0;
}