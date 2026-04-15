#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define PORT        9090
#define MAX_PAYLOAD 2048

typedef enum {
    SVR_DISPLAY = 1,
    SVR_PROMPT  = 2,
    CLI_INPUT   = 3
} MsgType;

typedef struct {
    int  type;
    char text[MAX_PAYLOAD];
} Msg;

int main(void) {
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if(connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        fprintf(stderr, "Make sure the server is running on port %d.\n", PORT);
        exit(1);
    }

    Msg  msg;
    char input[MAX_PAYLOAD];

    while(1) {\
        int n = recv(sock, &msg, sizeof(msg), 0);
        if(n <= 0) {
            printf("\n[client] Server disconnected.\n");
            break;
        }

        switch(msg.type) {

            case SVR_DISPLAY:
                printf("%s", msg.text);
                fflush(stdout);
                break;

            case SVR_PROMPT:
                printf("%s", msg.text);
                fflush(stdout);

                if(fgets(input, sizeof(input), stdin) == NULL) {
                    input[0] = '\n';
                    input[1] = '\0';
                }

                {
                    Msg reply;
                    memset(&reply, 0, sizeof(reply));
                    reply.type = CLI_INPUT;
                    strncpy(reply.text, input, MAX_PAYLOAD - 1);
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