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

void add_position(const char *position)
{
    FILE *f = fopen("positions.txt", "a");
    if (!f) return;

    fprintf(f, "%s\n", position);
    fclose(f);
}

void save_contestant(const char *name, const char *regNo, const char *position)
{
    FILE *f = fopen("contestants.txt", "a");
    if (!f) return;

    fprintf(f, "%s|%s|%s|0\n", name, regNo, position);
    fclose(f);
}

void get_tally_results(char *response)
{
    FILE *f = fopen("contestants.txt", "r");
    char line[256];
    char result[2000] = "--- Election Results ---\n";

    if (!f) {
        strcpy(response, "ERROR: No contestants found");
        return;
    }

    strcat(result, "Vote Counts:\n");
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *name = strtok(line, "|");
        char *regNo = strtok(NULL, "|");
        char *position = strtok(NULL, "|");
        char *votes = strtok(NULL, "|");

        if (name && regNo && position && votes) {
            char entry[256];
            snprintf(entry, sizeof(entry), "%s (%s) for %s - %d votes\n",
                     name, regNo, position, atoi(votes));
            strcat(result, entry);
        }
    }

    fclose(f);
    strcpy(response, result);
}

void get_admin_info(const char *regNo, const char *pwd, char *response)
{
    FILE *f = fopen("admin.txt", "r");
    char line[256];

    if (!f) {
        strcpy(response, "ERROR: Admin file not found");
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *name = strtok(line, "|");
        char *r = strtok(NULL, "|");
        char *p = strtok(NULL, "|");

        if (r && p && strcmp(r, regNo) == 0 && strcmp(p, pwd) == 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Admin: %s | Reg No: %s | Password: %s", name, r, p);
            strcpy(response, msg);
            fclose(f);
            return;
        }
    }

    fclose(f);
    strcpy(response, "ERROR: Admin not found");
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
         * ADD POSITION
         * ===================================================== */
        else if (strcmp(command, "ADD_POSITION") == 0) {
            char *regNo = strtok(NULL, "|");
            char *password = strtok(NULL, "|");
            char *position = strtok(NULL, "|");

            if (!regNo || !password || !position) {
                char *msg = "ERROR: Invalid position format";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            if (!verify_admin(regNo, password)) {
                char *msg = "ERROR: Admin not authenticated";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            add_position(position);
            char *msg = "SUCCESS: Position added";
            sendto(sock, msg, strlen(msg)+1, 0,
                   (struct sockaddr *)&client_addr, client_len);
        }

        /* =====================================================
         * REGISTER CONTESTANT
         * ===================================================== */
        else if (strcmp(command, "REGISTER_CONTESTANT") == 0) {
            char *regNo = strtok(NULL, "|");
            char *password = strtok(NULL, "|");
            char *name = strtok(NULL, "|");
            char *c_regNo = strtok(NULL, "|");
            char *position = strtok(NULL, "|");

            if (!regNo || !password || !name || !c_regNo || !position) {
                char *msg = "ERROR: Invalid contestant format";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            if (!verify_admin(regNo, password)) {
                char *msg = "ERROR: Admin not authenticated";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            save_contestant(name, c_regNo, position);
            char *msg = "SUCCESS: Contestant registered";
            sendto(sock, msg, strlen(msg)+1, 0,
                   (struct sockaddr *)&client_addr, client_len);
        }

        /* =====================================================
         * TALLY VOTES
         * ===================================================== */
        else if (strcmp(command, "TALLY_VOTES") == 0) {
            char *regNo = strtok(NULL, "|");
            char *password = strtok(NULL, "|");

            if (!regNo || !password) {
                char *msg = "ERROR: Invalid tally format";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            if (!verify_admin(regNo, password)) {
                char *msg = "ERROR: Admin not authenticated";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            char response[2000];
            get_tally_results(response);
            sendto(sock, response, strlen(response)+1, 0,
                   (struct sockaddr *)&client_addr, client_len);
        }

        /* =====================================================
         * GET ADMIN INFO
         * ===================================================== */
        else if (strcmp(command, "GET_ADMIN_INFO") == 0) {
            char *regNo = strtok(NULL, "|");
            char *password = strtok(NULL, "|");

            if (!regNo || !password) {
                char *msg = "ERROR: Invalid admin info format";
                sendto(sock, msg, strlen(msg)+1, 0,
                       (struct sockaddr *)&client_addr, client_len);
                continue;
            }

            char response[512];
            get_admin_info(regNo, password, response);
            sendto(sock, response, strlen(response)+1, 0,
                   (struct sockaddr *)&client_addr, client_len);
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