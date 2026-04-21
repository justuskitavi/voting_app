/* =========================================================
 * server_tcp.c  —  CONNECTION-ORIENTED ITERATIVE voting server
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT        9090
#define MAX_PAYLOAD 2048

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
    REQ_CREATE_ADMIN      = 101
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
    char positions[1000];     /* Newline-separated positions */
    int position_choice;      /* Index/choice of position */
} Msg;

struct Voter      { char name[50]; char regNo[20]; char password[20]; int voted; };
struct Contestant { char name[50]; char regNo[20]; char position[30]; int votes; };
struct Admin      { char name[50]; char regNo[20]; char password[20]; };

/* ================================================================== */
/*  Net helpers — STATELESS SERVER: only send responses               */
/* ================================================================== */

/* Send a response (resp_status and text) back to the client */
static void net_response(int sock, int status, const char *fmt, ...)
{
    Msg m; va_list ap;
    memset(&m, 0, sizeof(m));
    m.type        = 99;
    m.resp_status = status;
    va_start(ap, fmt);
    vsnprintf(m.text, MAX_PAYLOAD, fmt, ap);
    va_end(ap);
    send(sock, &m, sizeof(m), 0);
}

/* ================================================================== */
/*  Admin helpers — STATELESS                                          */
/* ================================================================== */
static int verify_admin_creds(const char *regNo, const char *password)
{
    FILE *f = fopen("admin.txt", "r");
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *name = strtok(line, "|");
        char *rn   = name ? strtok(NULL, "|") : NULL;
        char *rp   = rn   ? strtok(NULL, "|") : NULL;
        if (rn && rp &&
            strcmp(rn, regNo)    == 0 &&
            strcmp(rp, password) == 0) {
            fclose(f); return 1;
        }
    }
    fclose(f);
    return 0;
}

/* Check if admin exists */
static int admin_exists()
{
    FILE *f = fopen("admin.txt", "r");
    if (!f) return 0;
    char line[256];
    int exists = fgets(line, sizeof(line), f) != NULL;
    fclose(f);
    return exists;
}

/* ================================================================== */
/*  Admin sub-functions — STATELESS: process received data only       */
/* ================================================================== */
static void manage_positions(int sock, const Msg *req)
{
    if (!req->positions[0]) {
        net_response(sock, RESP_ERROR, "No positions provided.");
        return;
    }

    FILE *f = fopen("positions.txt", "w");  /* Overwrite with new list */
    if (!f) {
        net_response(sock, RESP_ERROR, "Error opening positions file.");
        return;
    }

    /* Parse positions (newline-separated) */
    char *pos_copy = strdup(req->positions);
    char *pos = strtok(pos_copy, "\n");
    while (pos) {
        if (strlen(pos) > 0)
            fprintf(f, "%s\n", pos);
        pos = strtok(NULL, "\n");
    }
    free(pos_copy);
    fclose(f);
    net_response(sock, RESP_SUCCESS, "Positions updated successfully.");
}

static void register_contestant(int sock, const Msg *req)
{
    if (!req->contestant_data.name[0] || !req->contestant_data.regNo[0] ||
        !req->contestant_data.position[0]) {
        net_response(sock, RESP_ERROR, "Incomplete contestant data.");
        return;
    }

    /* Verify position exists */
    FILE *pf = fopen("positions.txt", "r");
    if (!pf) {
        net_response(sock, RESP_ERROR, "No positions defined.");
        return;
    }

    int pos_found = 0;
    char line[256];
    while (fgets(line, sizeof(line), pf)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, req->contestant_data.position) == 0) {
            pos_found = 1;
            break;
        }
    }
    fclose(pf);

    if (!pos_found) {
        net_response(sock, RESP_ERROR, "Selected position not found.");
        return;
    }

    /* Add contestant */
    FILE *cf = fopen("contestants.txt", "a");
    if (!cf) {
        net_response(sock, RESP_ERROR, "Error opening contestants file.");
        return;
    }
    fprintf(cf, "%s|%s|%s|%d\n", req->contestant_data.name,
            req->contestant_data.regNo, req->contestant_data.position, 0);
    fclose(cf);

    net_response(sock, RESP_SUCCESS,
        "Contestant '%s' registered for position '%s'.",
        req->contestant_data.name, req->contestant_data.position);
}

static void tally_votes(int sock)
{
    FILE *f = fopen("contestants.txt", "r");
    if (!f) {
        net_response(sock, RESP_ERROR, "No contestants found.");
        return;
    }

    char line[256];
    int maxVotes = -1;
    char results[MAX_PAYLOAD] = "";

    /* First pass: collect all + find max */
    char *temp = malloc(MAX_PAYLOAD);
    strcpy(temp, "--- Election Results ---\n\nVote Counts:\n");

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *n  = strtok(line, "|");
        char *r  = strtok(NULL, "|");
        char *p  = strtok(NULL, "|");
        char *v  = strtok(NULL, "|");
        if (!n || !r || !p || !v) continue;
        int votes = atoi(v);
        char entry[256];
        snprintf(entry, sizeof(entry), "  %-20s (%-10s) for %-20s — %d votes\n",
                 n, r, p, votes);
        strcat(temp, entry);
        if (votes > maxVotes) maxVotes = votes;
    }

    /* Second pass: find winners */
    fseek(f, 0, SEEK_SET);
    strcat(temp, "\nWinner(s):\n");
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *n  = strtok(line, "|");
        char *r  = strtok(NULL, "|");
        char *p  = strtok(NULL, "|");
        char *v  = strtok(NULL, "|");
        if (!n || !r || !p || !v) continue;
        if (atoi(v) == maxVotes) {
            char winner[256];
            snprintf(winner, sizeof(winner), "  ★ %s (%s) for %s with %d vote(s)\n",
                     n, r, p, maxVotes);
            strcat(temp, winner);
        }
    }
    fclose(f);
    
    net_response(sock, RESP_SUCCESS, "%s", temp);
    free(temp);
}

static void view_admin_info(int sock, const Msg *req)
{
    if (!req->admin_data.regNo[0] || !req->admin_data.password[0]) {
        net_response(sock, RESP_ERROR, "Missing credentials.");
        return;
    }

    if (!verify_admin_creds(req->admin_data.regNo, req->admin_data.password)) {
        net_response(sock, RESP_ERROR, "Invalid admin credentials.");
        return;
    }

    FILE *f = fopen("admin.txt", "r");
    if (!f) {
        net_response(sock, RESP_ERROR, "Cannot open admin file.");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *name = strtok(line, "|");
        char *rn   = name ? strtok(NULL, "|") : NULL;
        if (name && rn && strcmp(rn, req->admin_data.regNo) == 0) {
            fclose(f);
            net_response(sock, RESP_SUCCESS, "Name   : %s\nReg No : %s", name, rn);
            return;
        }
    }
    fclose(f);
    net_response(sock, RESP_ERROR, "Admin record not found.");
}

/* Get list of available positions */
static void get_positions(int sock)
{
    FILE *f = fopen("positions.txt", "r");
    if (!f) {
        net_response(sock, RESP_ERROR, "No positions found.");
        return;
    }

    char *positions = malloc(MAX_PAYLOAD);
    positions[0] = '\0';
    char line[256];
    int count = 0;

    while (fgets(line, sizeof(line), f) && count < 100) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;
        if (count > 0) strcat(positions, "\n");
        strcat(positions, line);
        count++;
    }
    fclose(f);

    if (count == 0) {
        free(positions);
        net_response(sock, RESP_ERROR, "No positions defined.");
        return;
    }

    Msg resp;
    memset(&resp, 0, sizeof(resp));
    resp.type = 99;
    resp.resp_status = RESP_SUCCESS;
    resp.position_choice = count;  /* Store count in this field */
    strncpy(resp.text, positions, MAX_PAYLOAD - 1);
    send(sock, &resp, sizeof(resp), 0);

    free(positions);
}

/* ================================================================== */
/*  Voter registration (server-side, triggered by REQ_REGISTER_VOTER) */
/* ================================================================== */
static void handle_register_voter(int sock, const Msg *req)
{
    if (!strlen(req->voter_data.name)     ||
        !strlen(req->voter_data.regNo)    ||
        !strlen(req->voter_data.password)) {
        net_response(sock, RESP_ERROR, "Error: Empty fields in registration.");
        return;
    }

    /* Check for duplicate */
    FILE *f = fopen("voters.txt", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            strtok(line, "|");                    /* name  */
            char *rn = strtok(NULL, "|");         /* regNo */
            if (rn && strcmp(rn, req->voter_data.regNo) == 0) {
                fclose(f);
                net_response(sock, RESP_ERROR,
                    "Voter with Reg No '%s' already exists.",
                    req->voter_data.regNo);
                return;
            }
        }
        fclose(f);
    }

    f = fopen("voters.txt", "a");
    if (!f) { net_response(sock, RESP_ERROR, "Error opening voter file."); return; }
    fprintf(f, "%s|%s|%s|0\n",
            req->voter_data.name,
            req->voter_data.regNo,
            req->voter_data.password);
    fclose(f);
    net_response(sock, RESP_SUCCESS,
        "Voter '%s' registered successfully!", req->voter_data.name);
}

/* Admin create (server-side, triggered by REQ_CREATE_ADMIN) */
static void handle_create_admin(int sock, const Msg *req)
{
    if (!req->admin_data.name[0] || !req->admin_data.regNo[0] ||
        !req->admin_data.password[0]) {
        net_response(sock, RESP_ERROR, "Incomplete admin data.");
        return;
    }

    FILE *f = fopen("admin.txt", "w");
    if (!f) {
        net_response(sock, RESP_ERROR, "Error creating admin file.");
        return;
    }

    fprintf(f, "%s|%s|%s\n", req->admin_data.name, req->admin_data.regNo,
            req->admin_data.password);
    fclose(f);

    net_response(sock, RESP_SUCCESS, "Admin created successfully.");
}

/* ================================================================== */
/*  Main client handler                                                */
/* ================================================================== */
static void handle_client(int sock)
{
    printf("[tcp-server] Client connected.\n");

    Msg msg;
    while (1) {
        memset(&msg, 0, sizeof(msg));
        ssize_t n = recv(sock, &msg, sizeof(msg), 0);
        if (n <= 0) {
            printf("[tcp-server] Client disconnected.\n");
            break;
        }

        printf("[tcp-server] Request type: %d\n", msg.req_type);

        switch (msg.req_type) {
        case REQ_ENSURE_ADMIN:
            if (admin_exists()) {
                net_response(sock, RESP_SUCCESS, "Admin already exists.");
            } else {
                net_response(sock, RESP_NEED_INPUT, "Admin does not exist. Please provide admin data.");
            }
            break;

        case REQ_CREATE_ADMIN:
            handle_create_admin(sock, &msg);
            break;

        case REQ_VERIFY_ADMIN:
            if (verify_admin_creds(msg.admin_data.regNo, msg.admin_data.password)) {
                net_response(sock, RESP_SUCCESS, "Admin authenticated.");
            } else {
                net_response(sock, RESP_ERROR, "Invalid admin credentials.");
            }
            break;

        case REQ_REGISTER_VOTER:
            handle_register_voter(sock, &msg);
            break;

        case REQ_GET_POSITIONS:
            get_positions(sock);
            break;

        case REQ_MANAGE_POSITIONS:
            manage_positions(sock, &msg);
            break;

        case REQ_REG_CONTESTANT:
            register_contestant(sock, &msg);
            break;

        case REQ_TALLY_VOTES:
            tally_votes(sock);
            break;

        case REQ_VIEW_ADMIN_INFO:
            view_admin_info(sock, &msg);
            break;

        default:
            net_response(sock, RESP_ERROR, "Unknown request.");
            break;
        }
    }
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */
int main(void)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }

    if (listen(server_fd, 1) < 0) { perror("listen"); exit(1); }

    printf("[tcp-server] Listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        printf("[tcp-server] Client (%s:%d) connected.\n",
               ip, ntohs(client_addr.sin_port));

        handle_client(client_fd);

        close(client_fd);
        printf("[tcp-server] Client disconnected. Waiting...\n");
    }

    close(server_fd);
    return 0;
}