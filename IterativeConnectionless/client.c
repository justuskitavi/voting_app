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

    /* Send an empty CLI_INPUT to introduce ourselves and trigger the menu */
    {
        Msg init;
        memset(&init, 0, sizeof(init));
        init.type = CLI_INPUT;
        /* ★ send() works here because connect() set the default peer */
        send(sock, &init, sizeof(init), 0);
    }

    Msg  msg;
    char input[MAX_PAYLOAD];

    while (1) {
        /* ★ recv() — receives one UDP datagram from the server */
        ssize_t n = recv(sock, &msg, sizeof(msg), 0);
        if (n <= 0) {
            printf("\n[udp-client] No response (server may have stopped).\n");
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
                /* ★ Each send() dispatches an independent UDP datagram.
                 *   There is no stream; the server reassembles context
                 *   from its per-client session table. */
                send(sock, &reply, sizeof(reply), 0);
            }
            break;

        default:
            break;
        }
    }

    close(sock);
    return 0;
}