/* =========================================================
 * server_tcp.c  —  CONNECTION-ORIENTED ITERATIVE voting server
 *
 * CONNECTION-ORIENTED aspects (look for the ★ markers):
 *  ★ SOCK_STREAM  – TCP socket
 *  ★ listen()     – places socket in passive mode, allows the OS
 *                   to queue incoming connection requests
 *  ★ accept()     – blocks until a client completes the TCP
 *                   three-way handshake; returns a NEW socket
 *                   dedicated to that single client
 *  ★ recv()/send() on the per-client socket – stream I/O
 *  ★ close(client_fd) – tears down the TCP connection after
 *                        serving the client
 *  ★ ITERATIVE: the server handles ONE client fully before
 *    calling accept() again. No threads or forking.
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

typedef struct {
    int  type;
    char text[MAX_PAYLOAD];
} Msg;

struct Voter       { char name[50]; char regNo[20]; char password[20]; int voted; };
struct Contestant  { char name[50]; char regNo[20]; char position[30]; int votes; };
struct Admin       { char name[50]; char regNo[20]; char password[20]; };

/* ------------------------------------------------------------------ */
/*  Stream helpers — identical to original; work on the per-client fd  */
/* ------------------------------------------------------------------ */

static void net_print(int sock, const char *fmt, ...)
{
    Msg m; va_list ap;
    memset(&m, 0, sizeof(m));
    m.type = SVR_DISPLAY;
    va_start(ap, fmt); vsnprintf(m.text, MAX_PAYLOAD, fmt, ap); va_end(ap);
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

/* ------------------------------------------------------------------ */
/*  Admin helpers                                                       */
/* ------------------------------------------------------------------ */
static void ensure_admin_exists(int sock)
{
    FILE *f = fopen("admin.txt", "r");
    char  line[256];
    if (!f || !fgets(line, sizeof(line), f)) {
        if (f) fclose(f);
        /* create admin */
        net_print(sock, "\nNo admin found. Creating admin account.\n");
        f = fopen("admin.txt", "w");
        if (!f) { net_print(sock, "Error creating admin file.\n"); return; }
        struct Admin a;
        net_gets(sock, "Admin Name: ",       a.name,     sizeof(a.name));
        net_gets(sock, "Admin Reg No: ",     a.regNo,    sizeof(a.regNo));
        net_gets(sock, "Admin Password: ",   a.password, sizeof(a.password));
        fprintf(f, "%s|%s|%s\n", a.name, a.regNo, a.password);
        fclose(f);
        net_print(sock, "Admin created.\n");
        return;
    }
    fclose(f);
}

static int verify_admin(int sock)
{
    char inputRegNo[20], inputPwd[20];
    net_gets(sock, "Admin Reg No: ",  inputRegNo, sizeof(inputRegNo));
    net_gets(sock, "Admin Password: ", inputPwd,  sizeof(inputPwd));

    FILE *f = fopen("admin.txt", "r");
    if (!f) { net_print(sock, "Admin file missing.\n"); return 0; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *r = strtok(line, "|"); /* name */
        char *rn = r ? strtok(NULL, "|") : NULL;
        char *rp = rn ? strtok(NULL, "|") : NULL;
        if (rn && rp &&
            strcmp(rn, inputRegNo) == 0 &&
            strcmp(rp, inputPwd)   == 0) {
            fclose(f); return 1;
        }
    }
    fclose(f);
    net_print(sock, "Invalid admin credentials.\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Voter registration                                                  */
/* ------------------------------------------------------------------ */
static void register_voter(int sock)
{
    struct Voter v;
    net_print(sock, "\n--- Voter Registration ---\n");
    net_gets(sock, "Name: ",         v.name,     sizeof(v.name));
    net_gets(sock, "Reg No: ",       v.regNo,    sizeof(v.regNo));
    net_gets(sock, "Password: ",     v.password, sizeof(v.password));
    v.voted = 0;

    FILE *f = fopen("voters.txt", "a");
    if (!f) { net_print(sock, "Error opening voter file.\n"); return; }
    fprintf(f, "%s|%s|%s|%d\n", v.name, v.regNo, v.password, v.voted);
    fclose(f);
    net_print(sock, "Voter registered successfully!\n");
    net_pause(sock, "Press Enter to continue...");
}

/* ------------------------------------------------------------------ */
/*  Manage positions                                                    */
/* ------------------------------------------------------------------ */
static void manage_positions(int sock)
{
    FILE *f = fopen("positions.txt", "a");
    char  pos[30];
    if (!f) { net_print(sock, "Error opening positions file.\n"); return; }
    net_print(sock, "\n--- Manage Positions (empty line to finish) ---\n");
    while (1) {
        net_gets(sock, "Position: ", pos, sizeof(pos));
        if (strlen(pos) == 0) break;
        fprintf(f, "%s\n", pos);
    }
    fclose(f);
    net_print(sock, "Positions updated.\n");
}

/* ------------------------------------------------------------------ */
/*  Register contestant                                                 */
/* ------------------------------------------------------------------ */
static void register_contestant(int sock)
{
    char  line[256];
    char  positions[100][30];
    int   posCount = 0;
    FILE *pf = fopen("positions.txt", "r");
    if (!pf) { net_print(sock, "No positions found.\n"); return; }
    while (fgets(line, sizeof(line), pf) && posCount < 100) {
        line[strcspn(line, "\n")] = '\0';
        if (!strlen(line)) continue;
        strncpy(positions[posCount], line, 29);
        positions[posCount][29] = '\0';
        posCount++;
    }
    fclose(pf);
    if (!posCount) { net_print(sock, "No positions defined.\n"); return; }

    struct Contestant c;
    net_print(sock, "\n--- Contestant Registration ---\n");
    net_gets(sock, "Name: ",   c.name,  sizeof(c.name));
    net_gets(sock, "Reg No: ", c.regNo, sizeof(c.regNo));

    net_print(sock, "\nAvailable positions:\n");
    for (int i = 0; i < posCount; i++)
        net_print(sock, "%d. %s\n", i + 1, positions[i]);

    int choice = net_getint(sock, "Select position: ");
    if (choice < 1 || choice > posCount) {
        net_print(sock, "Invalid choice.\n"); return;
    }
    strncpy(c.position, positions[choice - 1], sizeof(c.position) - 1);
    c.votes = 0;

    FILE *cf = fopen("contestants.txt", "a");
    if (!cf) { net_print(sock, "Error opening contestants file.\n"); return; }
    fprintf(cf, "%s|%s|%s|%d\n", c.name, c.regNo, c.position, c.votes);
    fclose(cf);
    net_print(sock, "Contestant registered!\n");
    net_pause(sock, "Press Enter to continue...");
}

/* ------------------------------------------------------------------ */
/*  Tally votes                                                         */
/* ------------------------------------------------------------------ */
static void tally_votes(int sock)
{
    FILE *f = fopen("contestants.txt", "r");
    if (!f) { net_print(sock, "No contestants found.\n"); return; }

    char line[256];
    int  maxVotes = 0;
    struct Contestant c;

    net_print(sock, "\n--- Election Results ---\n\nVote Counts:\n");

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *n  = strtok(line, "|");
        char *r  = strtok(NULL, "|");
        char *p  = strtok(NULL, "|");
        char *v  = strtok(NULL, "|");
        if (!n || !r || !p || !v) continue;
        int votes = atoi(v);
        net_print(sock, "%s (%s) for %s — %d votes\n", n, r, p, votes);
        if (votes > maxVotes) maxVotes = votes;
    }

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
            net_print(sock, "%s (%s) for %s\n", n, r, p);
    }
    fclose(f);
    net_pause(sock, "\nPress Enter to return to menu...");
}

/* ------------------------------------------------------------------ */
/*  Cast vote                                                           */
/* ------------------------------------------------------------------ */
static void cast_vote(int sock)
{
    char regNo[20], password[20];
    char line[256];

    net_print(sock, "\n--- Cast Vote ---\n");
    net_gets(sock, "Reg No: ",   regNo,    sizeof(regNo));
    net_gets(sock, "Password: ", password, sizeof(password));

    /* verify voter */
    FILE *vf = fopen("voters.txt", "r");
    if (!vf) { net_print(sock, "Error opening voter file.\n"); return; }

    int found = 0, alreadyVoted = 0;
    struct Voter v;
    while (fgets(line, sizeof(line), vf)) {
        line[strcspn(line, "\n")] = '\0';
        char *n = strtok(line, "|");
        char *r = strtok(NULL, "|");
        char *p = strtok(NULL, "|");
        char *vt = strtok(NULL, "|");
        if (!n || !r || !p || !vt) continue;
        if (strcmp(r, regNo) == 0 && strcmp(p, password) == 0) {
            found = 1;
            if (atoi(vt) == 1) alreadyVoted = 1;
        }
    }
    fclose(vf);

    if (!found)        { net_print(sock, "Invalid credentials.\n"); return; }
    if (alreadyVoted)  { net_print(sock, "You have already voted.\n"); return; }

    /* load contestants */
    FILE *cf = fopen("contestants.txt", "r");
    if (!cf) { net_print(sock, "No contestants.\n"); return; }

    char names[100][50], regNos[100][20], pos[100][30];
    int  votes[100], count = 0;
    struct Contestant c;
    while (fgets(line, sizeof(line), cf) && count < 100) {
        line[strcspn(line, "\n")] = '\0';
        char *n = strtok(line, "|");
        char *r = strtok(NULL, "|");
        char *p = strtok(NULL, "|");
        char *vt = strtok(NULL, "|");
        if (!n || !r || !p || !vt) continue;
        strncpy(names[count], n, 49); names[count][49] = '\0';
        strncpy(regNos[count], r, 19); regNos[count][19] = '\0';
        strncpy(pos[count], p, 29); pos[count][29] = '\0';
        votes[count] = atoi(vt);
        count++;
    }
    fclose(cf);

    /* load positions */
    FILE *pf = fopen("positions.txt", "r");
    if (!pf) { net_print(sock, "No positions.\n"); return; }
    char positions[100][30]; int posCount = 0;
    while (fgets(line, sizeof(line), pf) && posCount < 100) {
        line[strcspn(line, "\n")] = '\0';
        if (!strlen(line)) continue;
        strncpy(positions[posCount], line, 29);
        positions[posCount][29] = '\0';
        posCount++;
    }
    fclose(pf);

    for (int pi = 0; pi < posCount; pi++) {
        int cIdx[100], cCount = 0;
        net_print(sock, "\nPosition: %s\n", positions[pi]);
        for (int j = 0; j < count; j++)
            if (strcmp(pos[j], positions[pi]) == 0) cIdx[cCount++] = j;
        if (!cCount) { net_print(sock, "No candidates.\n"); continue; }
        for (int k = 0; k < cCount; k++)
            net_print(sock, "%d. %s (%s)\n", k + 1, names[cIdx[k]], regNos[cIdx[k]]);
        int ch = net_getint(sock, "Vote (0 to skip): ");
        if (ch >= 1 && ch <= cCount) votes[cIdx[ch - 1]]++;
    }

    /* rewrite contestants */
    cf = fopen("contestants.txt", "w");
    if (cf) {
        for (int j = 0; j < count; j++)
            fprintf(cf, "%s|%s|%s|%d\n", names[j], regNos[j], pos[j], votes[j]);
        fclose(cf);
    }

    /* mark voter as voted */
    vf = fopen("voters.txt", "r");
    FILE *tmp = fopen("voters_temp.txt", "w");
    if (vf && tmp) {
        while (fgets(line, sizeof(line), vf)) {
            line[strcspn(line, "\n")] = '\0';
            char *n = strtok(line, "|");
            char *r = strtok(NULL, "|");
            char *p = strtok(NULL, "|");
            char *vt = strtok(NULL, "|");
            if (!n || !r || !p || !vt) continue;
            int voted = atoi(vt);
            if (strcmp(r, regNo) == 0 && strcmp(p, password) == 0) voted = 1;
            fprintf(tmp, "%s|%s|%s|%d\n", n, r, p, voted);
        }
        fclose(vf); fclose(tmp);
        remove("voters.txt");
        rename("voters_temp.txt", "voters.txt");
    }

    net_print(sock, "Vote cast successfully!\n");
}

/* ------------------------------------------------------------------ */
/*  Admin panel                                                         */
/* ------------------------------------------------------------------ */
static void admin_panel(int sock)
{
    net_print(sock, "\n--- Admin Panel ---\n");
    if (!verify_admin(sock)) {
        net_print(sock, "Authentication failed.\n"); return;
    }
    while (1) {
        net_print(sock,
            "\nAdmin Options:\n"
            "1. Manage Positions\n"
            "2. Register Contestant\n"
            "3. Tally Votes\n"
            "4. Back\n");
        int ch = net_getint(sock, "Choice: ");
        switch (ch) {
        case 1: manage_positions(sock);    break;
        case 2: register_contestant(sock); break;
        case 3: tally_votes(sock);         break;
        case 4: return;
        default: net_print(sock, "Invalid.\n");
        }
    }
}

/* ------------------------------------------------------------------ */
/*  handle_client — called for each accepted connection                 */
/* ------------------------------------------------------------------ */
static void handle_client(int sock)
{
    ensure_admin_exists(sock);

    while (1) {
        net_print(sock,
            "\nElectronic Voting System (TCP)\n"
            "1. Admin Panel\n"
            "2. Register Voter\n"
            "3. Cast Vote\n"
            "4. Exit\n");
        int ch = net_getint(sock, "Enter choice: ");
        switch (ch) {
        case 1: admin_panel(sock);    break;
        case 2: register_voter(sock); break;
        case 3: cast_vote(sock);      break;
        case 4:
            net_print(sock, "Goodbye!\n");
            return;
        default:
            net_print(sock, "Invalid choice.\n");
        }
    }
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    /* ★ CONNECTION-ORIENTED: SOCK_STREAM = TCP */
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

    /* ★ listen() — puts the socket in passive mode.
     *   The OS will complete the TCP three-way handshake on behalf of
     *   the server for up to 1 pending connection at a time. */
    if (listen(server_fd, 1) < 0) { perror("listen"); exit(1); }

    printf("[tcp-server] Listening on port %d (connection-oriented iterative)...\n", PORT);

    /* ★ CONNECTION-ORIENTED ITERATIVE server loop:
     *   accept() blocks until one client has completed the TCP handshake.
     *   It returns a NEW socket (client_fd) specific to that connection.
     *   We serve that client fully, then close client_fd and loop back
     *   to accept() for the next client. */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        /* ★ accept() — returns per-client socket; TCP handshake complete */
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        printf("[tcp-server] Client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        handle_client(client_fd);   /* serve the client (may be long) */

        /* ★ close() — tear down the TCP connection to this client */
        close(client_fd);

        printf("[tcp-server] Client disconnected. Waiting for next connection...\n");
        /* ★ Loop back to accept() — next client can now connect */
    }

    close(server_fd);
    return 0;
}