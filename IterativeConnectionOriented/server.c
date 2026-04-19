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
    REQ_ADMIN_BACK        = 90
} RequestType;

typedef enum {
    RESP_SUCCESS = 1,
    RESP_ERROR   = 2,
    RESP_DATA    = 3
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
} Msg;

struct Voter      { char name[50]; char regNo[20]; char password[20]; int voted; };
struct Contestant { char name[50]; char regNo[20]; char position[30]; int votes; };
struct Admin      { char name[50]; char regNo[20]; char password[20]; };

/* ================================================================== */
/*  Net helpers                                                        */
/* ================================================================== */
static void net_print(int sock, const char *fmt, ...)
{
    Msg m; va_list ap;
    memset(&m, 0, sizeof(m));
    m.type = SVR_DISPLAY;
    va_start(ap, fmt);
    vsnprintf(m.text, MAX_PAYLOAD, fmt, ap);
    va_end(ap);
    send(sock, &m, sizeof(m), 0);
}

static void net_gets(int sock, const char *prompt, char *buf, int size)
{
    Msg out, in;
    memset(&out, 0, sizeof(out));
    out.type = SVR_PROMPT;
    strncpy(out.text, prompt, MAX_PAYLOAD - 1);
    send(sock, &out, sizeof(out), 0);

    memset(&in, 0, sizeof(in));
    if (recv(sock, &in, sizeof(in), 0) <= 0) { buf[0] = '\0'; return; }
    strncpy(buf, in.text, size - 1);
    buf[size - 1] = '\0';
    buf[strcspn(buf, "\n")] = '\0';
}

static int net_getint(int sock, const char *prompt)
{
    char buf[64];
    net_gets(sock, prompt, buf, sizeof(buf));
    return atoi(buf);
}

static void net_pause(int sock, const char *msg_text)
{
    char dummy[8];
    net_gets(sock, msg_text, dummy, sizeof(dummy));
}

/* Send a final response (resp_status set) back to the client */
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
/*  Admin helpers                                                      */
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

/* Prompt over the network and verify — returns 1 on success */
static int verify_admin(int sock)
{
    char regNo[20], pwd[20];
    net_gets(sock, "Admin Reg No: ",   regNo, sizeof(regNo));
    net_gets(sock, "Admin Password: ", pwd,   sizeof(pwd));

    if (verify_admin_creds(regNo, pwd)) return 1;

    net_print(sock, "Invalid admin credentials.\n");
    return 0;
}

static void ensure_admin_exists(int sock)
{
    FILE *f    = fopen("admin.txt", "r");
    char  line[256];
    if (f && fgets(line, sizeof(line), f)) { fclose(f); return; }
    if (f) fclose(f);

    net_print(sock, "\nNo admin found. Please create an admin account.\n");

    f = fopen("admin.txt", "w");
    if (!f) { net_print(sock, "Error creating admin file.\n"); return; }

    struct Admin a;
    net_gets(sock, "Admin Name: ",     a.name,     sizeof(a.name));
    net_gets(sock, "Admin Reg No: ",   a.regNo,    sizeof(a.regNo));
    net_gets(sock, "Admin Password: ", a.password, sizeof(a.password));
    fprintf(f, "%s|%s|%s\n", a.name, a.regNo, a.password);
    fclose(f);
    net_print(sock, "Admin created successfully.\n");
}

/* ================================================================== */
/*  Admin sub-functions                                                */
/* ================================================================== */
static void manage_positions(int sock)
{
    FILE *f = fopen("positions.txt", "a");
    if (!f) { net_response(sock, RESP_ERROR, "Error opening positions file."); return; }

    net_print(sock, "\n--- Manage Positions ---\n");
    net_print(sock, "Enter position names (empty line to finish):\n");

    char pos[30];
    while (1) {
        net_gets(sock, "Position: ", pos, sizeof(pos));
        if (strlen(pos) == 0) break;
        fprintf(f, "%s\n", pos);
        net_print(sock, "Position '%s' added.\n", pos);
    }
    fclose(f);
    net_response(sock, RESP_SUCCESS, "Positions updated successfully.");
}

static void register_contestant(int sock)
{
    char line[256];
    char positions[100][30];
    int  posCount = 0;

    FILE *pf = fopen("positions.txt", "r");
    if (!pf) { net_response(sock, RESP_ERROR, "No positions found. Create positions first."); return; }

    while (fgets(line, sizeof(line), pf) && posCount < 100) {
        line[strcspn(line, "\n")] = '\0';
        if (!strlen(line)) continue;
        strncpy(positions[posCount], line, 29);
        positions[posCount][29] = '\0';
        posCount++;
    }
    fclose(pf);

    if (!posCount) { net_response(sock, RESP_ERROR, "No positions defined."); return; }

    struct Contestant c;
    net_print(sock, "\n--- Contestant Registration ---\n");
    net_gets(sock, "Name: ",   c.name,  sizeof(c.name));
    net_gets(sock, "Reg No: ", c.regNo, sizeof(c.regNo));

    /* Display available positions */
    net_print(sock, "\nAvailable Positions:\n");
    for (int i = 0; i < posCount; i++)
        net_print(sock, "%d. %s\n", i + 1, positions[i]);

    int choice = net_getint(sock, "Select position number: ");
    if (choice < 1 || choice > posCount) {
        net_response(sock, RESP_ERROR, "Invalid position choice."); return;
    }
    strncpy(c.position, positions[choice - 1], sizeof(c.position) - 1);
    c.position[sizeof(c.position) - 1] = '\0';
    c.votes = 0;

    FILE *cf = fopen("contestants.txt", "a");
    if (!cf) { net_response(sock, RESP_ERROR, "Error opening contestants file."); return; }
    fprintf(cf, "%s|%s|%s|%d\n", c.name, c.regNo, c.position, c.votes);
    fclose(cf);

    net_response(sock, RESP_SUCCESS,
        "Contestant '%s' registered for position '%s'.", c.name, c.position);
}

static void tally_votes(int sock)
{
    FILE *f = fopen("contestants.txt", "r");
    if (!f) { net_response(sock, RESP_ERROR, "No contestants found."); return; }

    char line[256];
    int  maxVotes = -1;

    net_print(sock, "\n--- Election Results ---\n\nVote Counts:\n");

    /* First pass: print all + find max */
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *n  = strtok(line, "|");
        char *r  = strtok(NULL, "|");
        char *p  = strtok(NULL, "|");
        char *v  = strtok(NULL, "|");
        if (!n || !r || !p || !v) continue;
        int votes = atoi(v);
        net_print(sock, "  %-20s (%-10s) for %-20s — %d votes\n",
                  n, r, p, votes);
        if (votes > maxVotes) maxVotes = votes;
    }

    /* Second pass: announce winner(s) */
    fseek(f, 0, SEEK_SET);
    net_print(sock, "\nWinner(s):\n");
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *n  = strtok(line, "|");
        char *r  = strtok(NULL, "|");
        char *p  = strtok(NULL, "|");
        char *v  = strtok(NULL, "|");
        if (!n || !r || !p || !v) continue;
        if (atoi(v) == maxVotes)
            net_print(sock, "  ★ %s (%s) for %s with %d vote(s)\n",
                      n, r, p, maxVotes);
    }
    fclose(f);
    net_response(sock, RESP_SUCCESS, "Tally complete.");
}

static void view_admin_info(int sock)
{
    net_print(sock, "\n--- Admin Info ---\n");

    char regNo[20], pwd[20];
    net_gets(sock, "Admin Reg No: ",   regNo, sizeof(regNo));
    net_gets(sock, "Admin Password: ", pwd,   sizeof(pwd));

    if (!verify_admin_creds(regNo, pwd)) {
        net_response(sock, RESP_ERROR, "Invalid admin credentials.");
        return;
    }

    FILE *f = fopen("admin.txt", "r");
    if (!f) { net_response(sock, RESP_ERROR, "Cannot open admin file."); return; }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *name = strtok(line, "|");
        char *rn   = name ? strtok(NULL, "|") : NULL;
        /* password intentionally not sent back over the wire */
        if (name && rn && strcmp(rn, regNo) == 0) {
            net_print(sock, "Name   : %s\n", name);
            net_print(sock, "Reg No : %s\n", rn);
            break;
        }
    }
    fclose(f);
    net_response(sock, RESP_SUCCESS, "Admin info displayed.");
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

/* ================================================================== */
/*  Admin panel — server side: verify first, then handle sub-requests */
/* ================================================================== */
static void handle_admin_panel(int sock)
{
    net_print(sock, "\n--- Admin Authentication ---\n");

    if (!verify_admin(sock)) {
        net_response(sock, RESP_ERROR, "Admin authentication failed.");
        return;
    }

    /* Auth succeeded — tell the client */
    net_response(sock, RESP_SUCCESS, "Admin authenticated successfully.");

    /*
     * Now wait for sub-menu requests from the client.
     * The client prints its own menu and sends REQ_* messages.
     * We serve each one and send a final net_response() back.
     * The loop ends when the client sends nothing more (disconnects or
     * picks "Back to Main Menu" — at that point the outer handle_client
     * loop just waits for the next top-level request).
     */
    Msg msg;
    while (1) {
        memset(&msg, 0, sizeof(msg));
        ssize_t n = recv(sock, &msg, sizeof(msg), 0);
        if (n <= 0) break;  /* client disconnected or went back */

        switch (msg.req_type) {
        case REQ_MANAGE_POSITIONS: manage_positions(sock);    break;
        case REQ_REG_CONTESTANT:   register_contestant(sock); break;
        case REQ_TALLY_VOTES:      tally_votes(sock);         break;
        case REQ_VIEW_ADMIN_INFO:  view_admin_info(sock);     break;
        case REQ_ADMIN_BACK:
            net_response(sock, RESP_SUCCESS, "Returning to main menu.");
            goto done;
        default:
            /* Not an admin sub-request — break back to main loop */
            net_response(sock, RESP_ERROR, "Invalid admin request.");
            break;
        }
    }
done:;
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
            ensure_admin_exists(sock);
            /* Send a plain response so pump_until_response() returns */
            net_response(sock, RESP_SUCCESS, "");
            break;

        case REQ_VERIFY_ADMIN:
            handle_admin_panel(sock);
            break;

        case REQ_REGISTER_VOTER:
            handle_register_voter(sock, &msg);
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