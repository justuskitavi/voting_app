/* =========================================================
 * server_udp_concurrent.c — CONCURRENT UDP server (fork)
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/file.h>

#define PORT 9090
#define BUFFER_SIZE 2048

/* ================= FILE LOCK ================= */
void lock_file(FILE *f) {
    if (f) flock(fileno(f), LOCK_EX);
}

void unlock_file(FILE *f) {
    if (f) flock(fileno(f), LOCK_UN);
}

/* ================= HELPERS ================= */

int verify_admin(const char *regNo, const char *pwd)
{
    FILE *f = fopen("admin.txt", "r");
    if (!f) return 0;
    lock_file(f);

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *name = strtok(line, "|");
        char *r = strtok(NULL, "|");
        char *p = strtok(NULL, "|");

        if (r && p && strcmp(r, regNo) == 0 && strcmp(p, pwd) == 0) {
            found = 1;
            break;
        }
    }

    unlock_file(f);
    fclose(f);
    return found;
}

int admin_exists()
{
    FILE *f = fopen("admin.txt", "r");
    if (!f) return 0;
    lock_file(f);
    char line[256];
    int exists = fgets(line, sizeof(line), f) != NULL;
    unlock_file(f);
    fclose(f);
    return exists;
}

int voter_exists(const char *regNo)
{
    FILE *f = fopen("voters.txt", "r");
    if (!f) return 0;
    lock_file(f);

    char line[256];
    int exists = 0;
    while (fgets(line, sizeof(line), f)) {
        char copy[256];
        strcpy(copy, line);
        strtok(copy, "|"); // name
        char *r = strtok(NULL, "|");
        if (r && strcmp(r, regNo) == 0) {
            exists = 1;
            break;
        }
    }

    unlock_file(f);
    fclose(f);
    return exists;
}

void save_voter(const char *name, const char *regNo, const char *pwd)
{
    FILE *f = fopen("voters.txt", "a");
    if (!f) return;
    lock_file(f);
    fprintf(f, "%s|%s|%s|0\n", name, regNo, pwd);
    unlock_file(f);
    fclose(f);
}

int verify_voter(const char *regNo, const char *pwd)
{
    FILE *f = fopen("voters.txt", "r");
    if (!f) return 0;
    lock_file(f);

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *name = strtok(line, "|");
        char *r = strtok(NULL, "|");
        char *p = strtok(NULL, "|");

        if (r && p && strcmp(r, regNo) == 0 && strcmp(p, pwd) == 0) {
            found = 1;
            break;
        }
    }

    unlock_file(f);
    fclose(f);
    return found;
}

int is_fully_voted(const char *regNo)
{
    FILE *f = fopen("voters.txt", "r");
    if (!f) return 0;
    lock_file(f);

    char line[256];
    int voted = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char copy[256]; strcpy(copy, line);
        char *name = strtok(copy, "|");
        char *r = strtok(NULL, "|");
        strtok(NULL, "|"); // pwd
        char *v = strtok(NULL, "|");

        if (r && strcmp(r, regNo) == 0) {
            if (v && strcmp(v, "1") == 0) voted = 1;
            break;
        }
    }

    unlock_file(f);
    fclose(f);
    return voted;
}

int voted_already_position(const char *regNo, const char *position)
{
    FILE *f = fopen("votes_cast.txt", "r");
    if (!f) return 0;
    lock_file(f);
    char line[256];
    int voted = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *r = strtok(line, "|");
        char *p = strtok(NULL, "|");
        if (r && p && strcmp(r, regNo) == 0 && strcmp(p, position) == 0) {
            voted = 1; break;
        }
    }
    unlock_file(f); fclose(f);
    return voted;
}

void add_position(const char *position)
{
    FILE *f = fopen("positions.txt", "a");
    if (!f) return;
    lock_file(f);
    fprintf(f, "%s\n", position);
    unlock_file(f);
    fclose(f);
}

void save_contestant(const char *name, const char *regNo, const char *position)
{
    FILE *f = fopen("contestants.txt", "a");
    if (!f) return;
    lock_file(f);
    fprintf(f, "%s|%s|%s|0\n", name, regNo, position);
    unlock_file(f);
    fclose(f);
}

void update_vote(const char *contestant, char *response)
{
    FILE *lock_f = fopen("contestants.lock", "w");
    lock_file(lock_f);

    FILE *f = fopen("contestants.txt", "r");
    FILE *tmp = fopen("contestants.tmp", "w");

    if (!f || !tmp) {
        strcpy(response, "ERROR: File error");
        if (f) fclose(f);
        if (tmp) fclose(tmp);
        unlock_file(lock_f);
        fclose(lock_f);
        return;
    }

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char copy[256];
        strcpy(copy, line);

        char *name = strtok(copy, "|");
        char *r = strtok(NULL, "|");
        char *pos = strtok(NULL, "|");
        char *votes = strtok(NULL, "|");

        if (r && strcmp(r, contestant) == 0) {
            int v = votes ? atoi(votes) + 1 : 1;
            fprintf(tmp, "%s|%s|%s|%d\n", name, r, pos, v);
            found = 1;
        } else {
            fprintf(tmp, "%s\n", line);
        }
    }

    fclose(f);
    fclose(tmp);

    if (!found) {
        remove("contestants.tmp");
        strcpy(response, "ERROR: Contestant not found");
    } else {
        rename("contestants.tmp", "contestants.txt");
        strcpy(response, "SUCCESS: Vote cast");
    }

    unlock_file(lock_f);
    fclose(lock_f);
}

void mark_voted_position(const char *regNo, const char *position) {
    FILE *f = fopen("votes_cast.txt", "a");
    if (f) {
        lock_file(f);
        fprintf(f, "%s|%s\n", regNo, position);
        unlock_file(f);
        fclose(f);
    }
}

void mark_fully_voted(const char *regNo)
{
    FILE *lock_f = fopen("voters.lock", "w");
    lock_file(lock_f);

    FILE *f = fopen("voters.txt", "r");
    FILE *tmp = fopen("voters.tmp", "w");

    if (f && tmp) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            char copy[256];
            strcpy(copy, line);

            char *name = strtok(copy, "|");
            char *r = strtok(NULL, "|");
            char *pw = strtok(NULL, "|");

            if (r && strcmp(r, regNo) == 0) {
                fprintf(tmp, "%s|%s|%s|1\n", name, r, pw);
            } else {
                fprintf(tmp, "%s\n", line);
            }
        }
        fclose(f);
        fclose(tmp);
        rename("voters.tmp", "voters.txt");
    } else {
        if (f) fclose(f);
        if (tmp) fclose(tmp);
    }

    unlock_file(lock_f);
    fclose(lock_f);
}

void get_tally_results(char *response)
{
    FILE *f = fopen("contestants.txt", "r");
    if (!f) {
        strcpy(response, "ERROR: No contestants found");
        return;
    }
    lock_file(f);

    char line[256];
    char result[BUFFER_SIZE] = "--- Election Results ---\nVote Counts:\n";
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

    unlock_file(f);
    fclose(f);
    strcpy(response, result);
}

void get_contestants_list(const char *filter, char *response)
{
    FILE *f = fopen("contestants.txt", "r");
    if (!f) {
        strcpy(response, "ERROR: No contestants registered");
        return;
    }
    lock_file(f);

    char line[256];
    char result[BUFFER_SIZE] = "";
    if (filter && strlen(filter) > 0) {
        snprintf(result, BUFFER_SIZE, "--- Contestants for %s ---\n", filter);
    } else {
        strcpy(result, "--- Registered Contestants ---\n");
    }

    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char copy[256]; strcpy(copy, line);
        char *name = strtok(copy, "|");
        char *regNo_c = strtok(NULL, "|");
        char *position = strtok(NULL, "|");

        if (name && regNo_c && position) {
            if (!filter || strlen(filter) == 0 || strcmp(position, filter) == 0) {
                char entry[256];
                snprintf(entry, sizeof(entry), "%d. %s (Reg: %s) - Position: %s\n",
                         ++count, name, regNo_c, position);
                strcat(result, entry);
            }
        }
    }

    unlock_file(f);
    fclose(f);

    if (count == 0 && filter && strlen(filter) > 0) {
        snprintf(response, BUFFER_SIZE, "No contestants for %s.", filter);
    } else if (count == 0) {
        strcpy(response, "ERROR: No contestants registered");
    } else {
        strcpy(response, result);
    }
}

/* ================= HANDLE REQUEST ================= */
void handle_request(int sock, char *buffer, struct sockaddr_in client, socklen_t len)
{
    char *cmd = strtok(buffer, "|");
    if (!cmd) return;

    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE);

    if (strcmp(cmd, "REGISTER") == 0) {
        char *name = strtok(NULL, "|");
        char *regNo = strtok(NULL, "|");
        char *password = strtok(NULL, "|");

        if (!name || !regNo || !password) {
            strcpy(response, "ERROR: Invalid format");
        } else if (voter_exists(regNo)) {
            strcpy(response, "ERROR: Voter already exists");
        } else {
            save_voter(name, regNo, password);
            strcpy(response, "SUCCESS: Voter registered");
        }
    }
    else if (strcmp(cmd, "VERIFY_ADMIN") == 0) {
        char *regNo = strtok(NULL, "|");
        char *password = strtok(NULL, "|");

        if (verify_admin(regNo, password)) {
            strcpy(response, "SUCCESS: Admin authenticated");
        } else {
            strcpy(response, "ERROR: Invalid credentials");
        }
    }
    else if (strcmp(cmd, "CHECK_ADMIN") == 0) {
        if (admin_exists()) {
            strcpy(response, "EXISTS");
        } else {
            strcpy(response, "NOT_EXISTS");
        }
    }
    else if (strcmp(cmd, "CREATE_ADMIN") == 0) {
        char *name = strtok(NULL, "|");
        char *regNo = strtok(NULL, "|");
        char *password = strtok(NULL, "|");

        if (!name || !regNo || !password) {
            strcpy(response, "ERROR: Invalid format");
        } else if (admin_exists()) {
            strcpy(response, "ERROR: Admin already exists");
        } else {
            FILE *f = fopen("admin.txt", "w");
            if (f) {
                lock_file(f);
                fprintf(f, "%s|%s|%s\n", name, regNo, password);
                unlock_file(f);
                fclose(f);
                strcpy(response, "SUCCESS: Admin created");
            } else {
                strcpy(response, "ERROR: File error");
            }
        }
    }
    else if (strcmp(cmd, "ADD_POSITION") == 0) {
        char *regNo = strtok(NULL, "|");
        char *password = strtok(NULL, "|");
        char *pos = strtok(NULL, "|");

        if (!verify_admin(regNo, password)) {
            strcpy(response, "ERROR: Admin authentication failed");
        } else {
            add_position(pos);
            strcpy(response, "SUCCESS: Position added");
        }
    }
    else if (strcmp(cmd, "GET_POSITIONS") == 0) {
        FILE *f = fopen("positions.txt", "r");
        if (!f) {
            strcpy(response, "ERROR: No positions defined");
        } else {
            lock_file(f);
            char line[100];
            while (fgets(line, sizeof(line), f)) strcat(response, line);
            unlock_file(f); fclose(f);
        }
    }
    else if (strcmp(cmd, "REGISTER_CONTESTANT") == 0) {
        char *regNo = strtok(NULL, "|");
        char *password = strtok(NULL, "|");
        char *name = strtok(NULL, "|");
        char *cRegNo = strtok(NULL, "|");
        char *pos = strtok(NULL, "|");

        if (!verify_admin(regNo, password)) {
            strcpy(response, "ERROR: Admin authentication failed");
        } else {
            save_contestant(name, cRegNo, pos);
            strcpy(response, "SUCCESS: Contestant registered");
        }
    }
    else if (strcmp(cmd, "VIEW_CONTESTANTS") == 0) {
        char *filter = strtok(NULL, "|");
        get_contestants_list(filter, response);
    }
    else if (strcmp(cmd, "TALLY_VOTES") == 0) {
        char *regNo = strtok(NULL, "|");
        char *password = strtok(NULL, "|");

        if (!verify_admin(regNo, password)) {
            strcpy(response, "ERROR: Admin authentication failed");
        } else {
            get_tally_results(response);
        }
    }
    else if (strcmp(cmd, "GET_ADMIN_INFO") == 0) {
        char *regNo = strtok(NULL, "|");
        char *password = strtok(NULL, "|");

        FILE *f = fopen("admin.txt", "r");
        if (!f) {
            strcpy(response, "ERROR: Admin not found");
        } else {
            lock_file(f);
            char line[256];
            int found = 0;
            while (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\n")] = '\0';
                char *n = strtok(line, "|");
                char *r = strtok(NULL, "|");
                char *p = strtok(NULL, "|");
                if (r && p && strcmp(r, regNo) == 0 && strcmp(p, password) == 0) {
                    snprintf(response, BUFFER_SIZE, "Admin: %s | Reg No: %s | Password: %s", n, r, p);
                    found = 1;
                    break;
                }
            }
            unlock_file(f);
            fclose(f);
            if (!found) strcpy(response, "ERROR: Invalid credentials");
        }
    }
    else if (strcmp(cmd, "CAST_VOTE") == 0) {
        char *regNo = strtok(NULL, "|");
        char *pwd = strtok(NULL, "|");
        char *contestant = strtok(NULL, "|");

        if (!regNo || !pwd || !contestant) {
            strcpy(response, "ERROR: Invalid format");
        } else if (is_fully_voted(regNo)) {
            strcpy(response, "ERROR: You have already completed your voting process");
        } else if (!verify_voter(regNo, pwd)) {
            strcpy(response, "ERROR: Invalid credentials");
        } else {
            // Find contestant's position
            char cPos[30] = "";
            FILE *cf_look = fopen("contestants.txt", "r");
            if (cf_look) {
                lock_file(cf_look);
                char line[256];
                while (fgets(line, sizeof(line), cf_look)) {
                    line[strcspn(line, "\n")] = '\0';
                    char copy[256]; strcpy(copy, line);
                    char *n = strtok(copy, "|");
                    char *r = strtok(NULL, "|");
                    char *p = strtok(NULL, "|");
                    if (r && strcmp(r, contestant) == 0) {
                        strncpy(cPos, p, 29);
                        break;
                    }
                }
                unlock_file(cf_look); fclose(cf_look);
            }

            if (strlen(cPos) == 0) {
                strcpy(response, "ERROR: Contestant not found");
            } else if (voted_already_position(regNo, cPos)) {
                snprintf(response, BUFFER_SIZE, "ERROR: You have already voted for %s", cPos);
            } else {
                update_vote(contestant, response);
                if (strncmp(response, "SUCCESS", 7) == 0) {
                    mark_voted_position(regNo, cPos);
                }
            }
        }
    }
    else if (strcmp(cmd, "MARK_VOTED") == 0) {
        char *regNo = strtok(NULL, "|");
        if (regNo) {
            mark_fully_voted(regNo);
            strcpy(response, "SUCCESS: Voter marked as voted");
        } else {
            strcpy(response, "ERROR: No Reg No provided");
        }
    }
    else {
        strcpy(response, "ERROR: Unknown command");
    }

    sendto(sock, response, strlen(response) + 1, 0, (struct sockaddr*)&client, len);
}

/* ================= MAIN ================= */
int main()
{
    signal(SIGCHLD, SIG_IGN);   // auto clean children

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server, client;
    socklen_t len = sizeof(client);

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("Concurrent UDP Server running on port %d...\n", PORT);

    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        ssize_t n = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr*)&client, &len);

        if (n <= 0) continue;

        pid_t pid = fork();

        if (pid == 0) {
            // CHILD handles request
            handle_request(sock, buffer, client, len);
            exit(0);
        }
        // parent immediately continues receiving
    }

    return 0;
}
