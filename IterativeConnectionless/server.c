/* =========================================================
 * server_udp_fixed.c — PURE UDP server (string protocol)
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 9090
#define BUFFER_SIZE 2048

/* =========================================================
 * FILE OPERATIONS
 * ========================================================= */

int verify_admin(const char *regNo, const char *pwd)
{
    FILE *f = fopen("admin.txt", "r");
    char line[256];

    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';

        char *name = strtok(line, "|");
        char *r = strtok(NULL, "|");
        char *p = strtok(NULL, "|");

        if (r && p && strcmp(r, regNo) == 0 && strcmp(p, pwd) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

int voter_exists(const char *regNo)
{
    FILE *f = fopen("voters.txt", "r");
    char line[256];

    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        char *name = strtok(line, "|");
        char *r = strtok(NULL, "|");

        if (r && strcmp(r, regNo) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

void save_voter(const char *name, const char *regNo, const char *pwd)
{
    FILE *f = fopen("voters.txt", "a");
    if (!f) return;

    fprintf(f, "%s|%s|%s|0\n", name, regNo, pwd);
    fclose(f);
}

/* =========================================================
 * MAIN SERVER
 * ========================================================= */

int main(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("[UDP Server] Listening on port %d...\n", PORT);

    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        /* ✅ Receive message */
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0,
                             (struct sockaddr *)&client_addr, &client_len);

        if (n <= 0) continue;

        printf("\n[Server] Received: %s\n", buffer);

        /* =====================================================
         * PARSE MESSAGE
         * ===================================================== */

        char *command = strtok(buffer, "|");

        if (!command) continue;

        /* =====================================================
         * REGISTER VOTER
         * ===================================================== */
        if (strcmp(command, "REGISTER") == 0) {
            char *name = strtok(NULL, "|");
            char *regNo = strtok(NULL, "|");
            char *password = strtok(NULL, "|");

            if (!name || !regNo || !password) {
                char *msg = "ERROR: Invalid registration format";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            if (voter_exists(regNo)) {
                char *msg = "ERROR: Voter already exists";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            save_voter(name, regNo, password);

            char *msg = "SUCCESS: Voter registered";
            sendto(sock, msg, strlen(msg)+1, 0,
                   (struct sockaddr *)&client_addr, client_len);
        }

        /* =====================================================
         * VERIFY ADMIN
         * ===================================================== */
        else if (strcmp(command, "VERIFY_ADMIN") == 0) {
            char *regNo = strtok(NULL, "|");
            char *password = strtok(NULL, "|");

            if (!regNo || !password) {
                char *msg = "ERROR: Invalid admin format";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            if (verify_admin(regNo, password)) {
                char *msg = "SUCCESS: Admin authenticated";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
            } else {
                char *msg = "ERROR: Invalid credentials";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
            }
        }

        /* =====================================================
         * UNKNOWN COMMAND
         * ===================================================== */
        else {
            char *msg = "ERROR: Unknown request";
            sendto(sock, msg, strlen(msg)+1, 0,
                   (struct sockaddr *)&client_addr, client_len);
        }
    }

    close(sock);
    return 0;
}