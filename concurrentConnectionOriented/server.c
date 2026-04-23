/* =========================================================
 * server_tcp_concurrent.c  —  TCP concurrent server (fork)
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/file.h>

#define PORT 9090
#define MAX_PAYLOAD 2048

/* ================= PROTOCOL (Matches client.c) ================= */

typedef enum { SVR_DISPLAY = 1, SVR_PROMPT = 2, CLI_INPUT = 3 } MsgType;

typedef enum {
    REQ_REGISTER_VOTER    = 10,
    REQ_VERIFY_ADMIN      = 20,
    REQ_CAST_VOTE         = 30,
    REQ_MANAGE_POSITIONS  = 40,
    REQ_REG_CONTESTANT    = 50,
    REQ_TALLY_VOTES       = 60,
    REQ_VIEW_ADMIN_INFO   = 70,
    REQ_ENSURE_ADMIN      = 80,
    REQ_ADMIN_BACK        = 90,
    REQ_GET_POSITIONS     = 100,
    REQ_CREATE_ADMIN      = 101,
    REQ_VIEW_CONTESTANTS  = 102,
    REQ_MARK_VOTED        = 103
} RequestType;

typedef enum {
    RESP_SUCCESS = 1,
    RESP_ERROR   = 2,
    RESP_DATA    = 3,
    RESP_NEED_INPUT = 4
} ResponseType;

typedef struct {
    int  type;
    int  req_type;
    int  resp_status;
    char text[MAX_PAYLOAD];
    struct {
        char name[50];
        char regNo[20];
        char password[20];
    } voter_data;
    struct {
        char name[50];
        char regNo[20];
        char password[20];
    } admin_data;
    struct {
        char name[50];
        char regNo[20];
        char position[30];
    } contestant_data;
    char positions[1000];
    int position_choice;
} Msg;

/* ================= FILE LOCKING ================= */

void lock_file(FILE *f) {
    if (f) flock(fileno(f), LOCK_EX);
}

void unlock_file(FILE *f) {
    if (f) flock(fileno(f), LOCK_UN);
}

/* ================= HELPERS ================= */

int admin_exists() {
    FILE *f = fopen("admin.txt", "r");
    if (!f) return 0;
    char line[256];
    int exists = (fgets(line, sizeof(line), f) != NULL);
    fclose(f);
    return exists;
}

int verify_admin(const char *regNo, const char *pwd) {
    FILE *f = fopen("admin.txt", "r");
    if (!f) return 0;
    lock_file(f);
    char line[256];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *n = strtok(line, "|");
        char *r = strtok(NULL, "|");
        char *p = strtok(NULL, "|");
        if (r && p && strcmp(r, regNo) == 0 && strcmp(p, pwd) == 0) {
            ok = 1; break;
        }
    }
    unlock_file(f); fclose(f);
    return ok;
}

int voter_exists(const char *regNo) {
    FILE *f = fopen("voters.txt", "r");
    if (!f) return 0;
    lock_file(f);
    char line[256];
    int exists = 0;
    while (fgets(line, sizeof(line), f)) {
        char copy[256]; strcpy(copy, line);
        strtok(copy, "|");
        char *r = strtok(NULL, "|");
        if (r && strcmp(r, regNo) == 0) { exists = 1; break; }
    }
    unlock_file(f); fclose(f);
    return exists;
}

int is_fully_voted(const char *regNo) {
    FILE *f = fopen("voters.txt", "r");
    if (!f) return 0;
    lock_file(f);
    char line[256];
    int fully_voted = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char copy[256]; strcpy(copy, line);
        char *n = strtok(copy, "|");
        char *r = strtok(NULL, "|");
        strtok(NULL, "|"); // password
        char *v = strtok(NULL, "|");
        if (r && strcmp(r, regNo) == 0) {
            if (v && strcmp(v, "1") == 0) fully_voted = 1;
            break;
        }
    }
    unlock_file(f); fclose(f);
    return fully_voted;
}

int voted_already_position(const char *regNo, const char *position) {
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

void mark_voted_position(const char *regNo, const char *position) {
    FILE *f = fopen("votes_cast.txt", "a");
    if (f) {
        lock_file(f);
        fprintf(f, "%s|%s\n", regNo, position);
        unlock_file(f);
        fclose(f);
    }
}

void mark_fully_voted(const char *regNo) {
    FILE *lock_f = fopen("voters.lock", "w");
    lock_file(lock_f);

    FILE *f = fopen("voters.txt", "r");
    FILE *tmp = fopen("voters.tmp", "w");

    if (f && tmp) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            char copy[256]; strcpy(copy, line);
            char *n = strtok(copy, "|");
            char *r = strtok(NULL, "|");
            char *p = strtok(NULL, "|");
            if (r && strcmp(r, regNo) == 0) {
                fprintf(tmp, "%s|%s|%s|1\n", n, r, p);
            } else {
                fprintf(tmp, "%s\n", line);
            }
        }
        fclose(f); fclose(tmp);
        rename("voters.tmp", "voters.txt");
    } else {
        if (f) fclose(f);
        if (tmp) fclose(tmp);
    }
    unlock_file(lock_f); fclose(lock_f);
}

/* ================= SEND UTILITY ================= */

void send_msg(int sock, int status, const char *fmt, ...) {
    Msg m;
    memset(&m, 0, sizeof(m));
    m.resp_status = status;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m.text, MAX_PAYLOAD, fmt, ap);
    va_end(ap);
    send(sock, &m, sizeof(m), 0);
}

/* ================= HANDLERS ================= */

void handle_client(int sock) {
    Msg msg;
    while (recv(sock, &msg, sizeof(msg), 0) > 0) {
        printf("[child %d] Request: %d\n", getpid(), msg.req_type);
        Msg resp;
        memset(&resp, 0, sizeof(resp));

        switch (msg.req_type) {
            case REQ_ENSURE_ADMIN:
                if (!admin_exists()) {
                    send_msg(sock, RESP_NEED_INPUT, "No admin found. Please create one.");
                } else {
                    send_msg(sock, RESP_SUCCESS, "Admin exists.");
                }
                break;

            case REQ_CREATE_ADMIN:
                if (admin_exists()) {
                    send_msg(sock, RESP_ERROR, "Admin already exists.");
                } else {
                    FILE *f = fopen("admin.txt", "w");
                    if (f) {
                        lock_file(f);
                        fprintf(f, "%s|%s|%s\n", msg.admin_data.name, msg.admin_data.regNo, msg.admin_data.password);
                        unlock_file(f); fclose(f);
                        send_msg(sock, RESP_SUCCESS, "Admin created successfully.");
                    } else {
                        send_msg(sock, RESP_ERROR, "Server file error.");
                    }
                }
                break;

            case REQ_VERIFY_ADMIN:
                if (verify_admin(msg.admin_data.regNo, msg.admin_data.password)) {
                    send_msg(sock, RESP_SUCCESS, "Access granted.");
                } else {
                    send_msg(sock, RESP_ERROR, "Invalid admin credentials.");
                }
                break;

            case REQ_REGISTER_VOTER:
                if (voter_exists(msg.voter_data.regNo)) {
                    send_msg(sock, RESP_ERROR, "Voter already registered.");
                } else {
                    FILE *f = fopen("voters.txt", "a");
                    if (f) {
                        lock_file(f);
                        fprintf(f, "%s|%s|%s|0\n", msg.voter_data.name, msg.voter_data.regNo, msg.voter_data.password);
                        unlock_file(f); fclose(f);
                        send_msg(sock, RESP_SUCCESS, "Voter registered.");
                    } else {
                        send_msg(sock, RESP_ERROR, "Server file error.");
                    }
                }
                break;

            case REQ_MANAGE_POSITIONS: {
                FILE *f = fopen("positions.txt", "a");
                if (f) {
                    lock_file(f);
                    fprintf(f, "%s\n", msg.positions);
                    unlock_file(f); fclose(f);
                    send_msg(sock, RESP_SUCCESS, "Positions updated.");
                } else {
                    send_msg(sock, RESP_ERROR, "File error.");
                }
                break;
            }

            case REQ_GET_POSITIONS: {
                FILE *f = fopen("positions.txt", "r");
                if (!f) {
                    send_msg(sock, RESP_ERROR, "No positions defined.");
                } else {
                    lock_file(f);
                    char content[1000] = "";
                    char line[100];
                    while (fgets(line, sizeof(line), f)) strcat(content, line);
                    unlock_file(f); fclose(f);
                    send_msg(sock, RESP_SUCCESS, "%s", content);
                }
                break;
            }

            case REQ_REG_CONTESTANT: {
                FILE *f = fopen("contestants.txt", "a");
                if (f) {
                    lock_file(f);
                    fprintf(f, "%s|%s|%s|0\n", msg.contestant_data.name, msg.contestant_data.regNo, msg.contestant_data.position);
                    unlock_file(f); fclose(f);
                    send_msg(sock, RESP_SUCCESS, "Contestant registered.");
                } else {
                    send_msg(sock, RESP_ERROR, "File error.");
                }
                break;
            }

            case REQ_VIEW_CONTESTANTS: {
                FILE *f = fopen("contestants.txt", "r");
                if (!f) {
                    send_msg(sock, RESP_SUCCESS, "No contestants registered.");
                } else {
                    lock_file(f);
                    char filter[30];
                    strncpy(filter, msg.contestant_data.position, 29);
                    filter[29] = '\0';

                    char list[MAX_PAYLOAD] = "";
                    if (strlen(filter) > 0) {
                        snprintf(list, sizeof(list), "--- Contestants for %s ---\n", filter);
                    } else {
                        strcpy(list, "--- All Contestants ---\n");
                    }
                    
                    char line[256];
                    int found = 0;
                    while (fgets(line, sizeof(line), f)) {
                        char copy[256]; strcpy(copy, line);
                        char *n = strtok(copy, "|");
                        char *r = strtok(NULL, "|");
                        char *p = strtok(NULL, "|");
                        if (n && r && p) {
                            if (strlen(filter) == 0 || strcmp(p, filter) == 0) {
                                char entry[100];
                                snprintf(entry, sizeof(entry), "%s (%s) - %s\n", n, r, p);
                                strcat(list, entry);
                                found++;
                            }
                        }
                    }
                    unlock_file(f); fclose(f);
                    if (found == 0 && strlen(filter) > 0) {
                        send_msg(sock, RESP_SUCCESS, "No contestants for %s.", filter);
                    } else {
                        send_msg(sock, RESP_SUCCESS, "%s", list);
                    }
                }
                break;
            }

            case REQ_CAST_VOTE: {
                char *vReg = msg.voter_data.regNo;
                char *vPwd = msg.voter_data.password;
                char *cReg = msg.contestant_data.regNo;

                if (is_fully_voted(vReg)) {
                    send_msg(sock, RESP_ERROR, "You have already completed your voting process.");
                    break;
                }

                FILE *vFile = fopen("voters.txt", "r");
                int validVoter = 0;
                if (vFile) {
                    lock_file(vFile);
                    char line[256];
                    while (fgets(line, sizeof(line), vFile)) {
                        line[strcspn(line, "\n")] = '\0';
                        char *n = strtok(line, "|");
                        char *r = strtok(NULL, "|");
                        char *p = strtok(NULL, "|");
                        if (r && p && strcmp(r, vReg) == 0 && strcmp(p, vPwd) == 0) {
                            validVoter = 1; break;
                        }
                    }
                    unlock_file(vFile); fclose(vFile);
                }

                if (!validVoter) {
                    send_msg(sock, RESP_ERROR, "Invalid voter credentials.");
                    break;
                }

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
                        if (r && strcmp(r, cReg) == 0) {
                            strncpy(cPos, p, 29);
                            break;
                        }
                    }
                    unlock_file(cf_look); fclose(cf_look);
                }

                if (strlen(cPos) == 0) {
                    send_msg(sock, RESP_ERROR, "Contestant not found.");
                    break;
                }

                if (voted_already_position(vReg, cPos)) {
                    send_msg(sock, RESP_ERROR, "You have already voted for %s.", cPos);
                } else {
                    // Update vote
                    FILE *lock_c = fopen("contestants.lock", "w");
                    lock_file(lock_c);
                    FILE *cf = fopen("contestants.txt", "r");
                    FILE *ct = fopen("contestants.tmp", "w");
                    int found = 0;
                    if (cf && ct) {
                        char line[256];
                        while (fgets(line, sizeof(line), cf)) {
                            line[strcspn(line, "\n")] = '\0';
                            char copy[256]; strcpy(copy, line);
                            char *n = strtok(copy, "|");
                            char *r = strtok(NULL, "|");
                            char *p = strtok(NULL, "|");
                            char *v = strtok(NULL, "|");
                            if (r && strcmp(r, cReg) == 0) {
                                fprintf(ct, "%s|%s|%s|%d\n", n, r, p, atoi(v) + 1);
                                found = 1;
                            } else { fprintf(ct, "%s\n", line); }
                        }
                        fclose(cf); fclose(ct);
                        if (found) rename("contestants.tmp", "contestants.txt");
                        else remove("contestants.tmp");
                    }
                    unlock_file(lock_c); fclose(lock_c);

                    if (found) {
                        mark_voted_position(vReg, cPos);
                        send_msg(sock, RESP_SUCCESS, "Vote cast successfully for %s.", cPos);
                    } else {
                        send_msg(sock, RESP_ERROR, "Contestant disappeared!");
                    }
                }
                break;
            }

            case REQ_TALLY_VOTES: {
                FILE *f = fopen("contestants.txt", "r");
                if (!f) {
                    send_msg(sock, RESP_ERROR, "No results available.");
                } else {
                    lock_file(f);
                    char res[MAX_PAYLOAD] = "--- Tally Results ---\n";
                    char line[256];
                    while (fgets(line, sizeof(line), f)) {
                        char copy[256]; strcpy(copy, line);
                        char *n = strtok(copy, "|");
                        char *r = strtok(NULL, "|");
                        char *p = strtok(NULL, "|");
                        char *v = strtok(NULL, "|");
                        if (n && v) {
                            char entry[100];
                            snprintf(entry, sizeof(entry), "%s (%s): %s votes\n", n, p, v);
                            strcat(res, entry);
                        }
                    }
                    unlock_file(f); fclose(f);
                    send_msg(sock, RESP_SUCCESS, "%s", res);
                }
                break;
            }

            case REQ_VIEW_ADMIN_INFO: {
                FILE *f = fopen("admin.txt", "r");
                if (!f) {
                    send_msg(sock, RESP_ERROR, "Admin file missing.");
                } else {
                    lock_file(f);
                    char line[256], info[256] = "Admin not found.";
                    while (fgets(line, sizeof(line), f)) {
                        line[strcspn(line, "\n")] = '\0';
                        char copy[256]; strcpy(copy, line);
                        char *n = strtok(copy, "|");
                        char *r = strtok(NULL, "|");
                        char *p = strtok(NULL, "|");
                        if (r && strcmp(r, msg.admin_data.regNo) == 0) {
                            snprintf(info, sizeof(info), "Name: %s | Reg: %s | Pwd: %s", n, r, p);
                            break;
                        }
                    }
                    unlock_file(f); fclose(f);
                    send_msg(sock, RESP_SUCCESS, "%s", info);
                }
                break;
            }

            case REQ_MARK_VOTED:
                mark_fully_voted(msg.voter_data.regNo);
                send_msg(sock, RESP_SUCCESS, "Voter marked as fully voted.");
                break;

            default:
                send_msg(sock, RESP_ERROR, "Command not implemented.");
                break;
        }
    }
    close(sock);
    exit(0);
}

void reap_zombies(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(1);
    }

    if (listen(server_fd, 10) < 0) { perror("listen"); exit(1); }

    signal(SIGCHLD, reap_zombies);

    printf("[TCP Server] Listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &len);
        if (client_sock < 0) { perror("accept"); continue; }

        printf("[server] Connection from %s\n", inet_ntoa(client_addr.sin_addr));

        if (fork() == 0) {
            close(server_fd);
            handle_client(client_sock);
        } else {
            close(client_sock);
        }
    }

    return 0;
}
