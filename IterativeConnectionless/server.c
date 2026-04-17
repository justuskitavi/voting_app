/* =========================================================
 * server_udp.c  —  CONNECTIONLESS ITERATIVE voting server
 *
 * CONNECTIONLESS aspects (look for the ★ markers):
 *  ★ SOCK_DGRAM  – UDP socket, not TCP
 *  ★ No listen() / accept() – there is no "connection"
 *  ★ recvfrom()  – receives one datagram AND captures the
 *                   client's address in one call
 *  ★ sendto()    – sends a datagram directly to that address
 *  ★ The server loops forever reading datagrams; any client
 *    can send at any time with no prior handshake
 *
 * Application protocol
 *  Client sends a Msg (type + text).
 *  Server replies with one or more Msg packets.
 *  Because UDP is message-based each Msg fits in one datagram.
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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

/* ---- session state stored per-client via a tiny table ---- */
typedef struct {
    struct sockaddr_in addr;
    int  active;
    int  step;          /* tracks where in a multi-step flow we are */
    char regNo[20];
    char password[20];
    char name[50];
    int  awaitingInput; /* 1 = server sent a prompt, waiting for reply */
} ClientSession;

#define MAX_SESSIONS 64
static ClientSession sessions[MAX_SESSIONS];

/* ------------------------------------------------------------------ */
/*  File-format helpers (same as original)                             */
/* ------------------------------------------------------------------ */
struct Voter       { char name[50]; char regNo[20]; char password[20]; int voted; };
struct Contestant  { char name[50]; char regNo[20]; char position[30]; int votes; };
struct Admin       { char name[50]; char regNo[20]; char password[20]; };

/* ------------------------------------------------------------------ */
/*  Network helpers                                                     */
/* ------------------------------------------------------------------ */

/* ★ CONNECTIONLESS: send a display message to a specific client address */
static void net_send_display(int sock,
                             const struct sockaddr_in *caddr,
                             const char *fmt, ...)
{
    Msg m;
    va_list ap;
    memset(&m, 0, sizeof(m));
    m.type = SVR_DISPLAY;
    va_start(ap, fmt);
    vsnprintf(m.text, MAX_PAYLOAD, fmt, ap);
    va_end(ap);
    /* ★ sendto() – no connected socket, we specify the destination each time */
    sendto(sock, &m, sizeof(m), 0,
           (const struct sockaddr *)caddr, sizeof(*caddr));
}

/* ★ CONNECTIONLESS: send a prompt and record that we are waiting for input */
static void net_send_prompt(int sock,
                            const struct sockaddr_in *caddr,
                            const char *prompt)
{
    Msg m;
    memset(&m, 0, sizeof(m));
    m.type = SVR_PROMPT;
    strncpy(m.text, prompt, MAX_PAYLOAD - 1);
    sendto(sock, &m, sizeof(m), 0,
           (const struct sockaddr *)caddr, sizeof(*caddr));
}

/* ------------------------------------------------------------------ */
/*  Session lookup / creation                                           */
/* ------------------------------------------------------------------ */
static ClientSession *find_or_create_session(const struct sockaddr_in *addr)
{
    int   free_slot = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active &&
            sessions[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            sessions[i].addr.sin_port        == addr->sin_port)
            return &sessions[i];
        if (!sessions[i].active && free_slot < 0)
            free_slot = i;
    }
    if (free_slot < 0) return NULL;   /* table full */
    memset(&sessions[free_slot], 0, sizeof(sessions[free_slot]));
    sessions[free_slot].active = 1;
    sessions[free_slot].addr   = *addr;
    sessions[free_slot].step   = 0;
    return &sessions[free_slot];
}

/* ------------------------------------------------------------------ */
/*  Voting logic (file I/O unchanged from original)                    */
/* ------------------------------------------------------------------ */
static int admin_exists(void)
{
    FILE *f = fopen("admin.txt", "r");
    char  line[256];
    if (!f) return 0;
    int ok = (fgets(line, sizeof(line), f) != NULL);
    fclose(f);
    return ok;
}

static int verify_admin(const char *regNo, const char *pwd)
{
    FILE *f = fopen("admin.txt", "r");
    char  line[256];
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *n = strtok(line, "|");
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

static void save_voter(const char *name, const char *regNo, const char *pwd)
{
    FILE *f = fopen("voters.txt", "a");
    if (f) { fprintf(f, "%s|%s|%s|0\n", name, regNo, pwd); fclose(f); }
}

/* ------------------------------------------------------------------ */
/*  State-machine dispatcher                                            */
/*  Because UDP carries no connection state, all multi-step flows      */
/*  are encoded as numbered steps inside ClientSession.step.           */
/* ------------------------------------------------------------------ */
#define STEP_MAIN_MENU         0
#define STEP_REGISTER_NAME     10
#define STEP_REGISTER_REGNO    11
#define STEP_REGISTER_PWD      12
#define STEP_ADMIN_REGNO       20
#define STEP_ADMIN_PWD         21
#define STEP_GOODBYE           99

static void send_main_menu(int sock, const struct sockaddr_in *addr)
{
    net_send_prompt(sock, addr,
        "\nElectronic Voting System (UDP)\n"
        "1. Admin Panel\n"
        "2. Register Voter\n"
        "3. Exit\n"
        "Enter choice: ");
}

static void dispatch(int sock,
                     ClientSession *cs,
                     const char *input)
{
    const struct sockaddr_in *addr = &cs->addr;

    switch (cs->step) {

    /* ---- main menu ---- */
    case STEP_MAIN_MENU:
        if (strcmp(input, "1") == 0) {
            if (!admin_exists()) {
                net_send_display(sock, addr, "No admin found.\n");
                send_main_menu(sock, addr);
            } else {
                cs->step = STEP_ADMIN_REGNO;
                net_send_prompt(sock, addr, "Admin Reg No: ");
            }
        } else if (strcmp(input, "2") == 0) {
            cs->step = STEP_REGISTER_NAME;
            net_send_prompt(sock, addr, "Voter name: ");
        } else if (strcmp(input, "3") == 0) {
            net_send_display(sock, addr, "Goodbye!\n");
            printf("[udp-server] Client (%s:%d) has disconnected.\n",
                   inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
            cs->active = 0;   /* remove session */
        } else {
            net_send_display(sock, addr, "Invalid choice.\n");
            send_main_menu(sock, addr);
        }
        break;

    /* ---- voter registration ---- */
    case STEP_REGISTER_NAME:
        strncpy(cs->name, input, sizeof(cs->name) - 1);
        cs->step = STEP_REGISTER_REGNO;
        net_send_prompt(sock, addr, "Registration number: ");
        break;

    case STEP_REGISTER_REGNO:
        strncpy(cs->regNo, input, sizeof(cs->regNo) - 1);
        cs->step = STEP_REGISTER_PWD;
        net_send_prompt(sock, addr, "Password: ");
        break;

    case STEP_REGISTER_PWD:
        save_voter(cs->name, cs->regNo, input);
        net_send_display(sock, addr, "Voter registered successfully!\n");
        cs->step = STEP_MAIN_MENU;
        send_main_menu(sock, addr);
        break;

    /* ---- admin authentication ---- */
    case STEP_ADMIN_REGNO:
        strncpy(cs->regNo, input, sizeof(cs->regNo) - 1);
        cs->step = STEP_ADMIN_PWD;
        net_send_prompt(sock, addr, "Admin password: ");
        break;

    case STEP_ADMIN_PWD:
        if (verify_admin(cs->regNo, input)) {
            net_send_display(sock, addr, "Admin authenticated. (Full admin panel not shown for brevity.)\n");
        } else {
            net_send_display(sock, addr, "Invalid admin credentials.\n");
        }
        cs->step = STEP_MAIN_MENU;
        send_main_menu(sock, addr);
        break;

    default:
        cs->step = STEP_MAIN_MENU;
        send_main_menu(sock, addr);
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    /* ★ CONNECTIONLESS: SOCK_DGRAM = UDP */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }

    printf("[udp-server] Listening on port %d (connectionless iterative)...\n", PORT);

    /* ★ CONNECTIONLESS ITERATIVE server loop:
     *   No listen() — no listen backlog.
     *   No accept() — no per-client socket is created.
     *   We sit in a single loop calling recvfrom() which gives us
     *   both the datagram and the sender's address. */
    while (1) {
        Msg                msg;
        struct sockaddr_in client_addr;
        socklen_t          client_len = sizeof(client_addr);

        /* ★ recvfrom() – blocks until ANY client sends a datagram */
        ssize_t n = recvfrom(sock, &msg, sizeof(msg), 0,
                             (struct sockaddr *)&client_addr, &client_len);
        if (n <= 0) continue;

        /* Ignore anything that isn't a CLI_INPUT message */
        if (msg.type != CLI_INPUT) continue;

        /* Remove trailing newline */
        msg.text[strcspn(msg.text, "\n")] = '\0';

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int  client_port = ntohs(client_addr.sin_port);

        /* Look up (or create) a session for this client IP:port */
        int is_new = 0;
        ClientSession *cs = find_or_create_session(&client_addr);
        if (!cs) {
            /* No room — tell the client and move on */
            net_send_display(sock, &client_addr, "Server full. Try again later.\n");
            continue;
        }

        /* Detect first datagram from this client (step still 0 and session just created) */
        if (cs->step == 0 && msg.text[0] == '\0') is_new = 1;

        if (is_new)
            printf("[udp-server] Client (%s:%d) has connected.\n",
                   client_ip, client_port);
        else
            printf("[udp-server] Datagram from (%s:%d)  step=%d  input='%s'\n",
                   client_ip, client_port, cs->step, msg.text);

        /* If this is a brand-new session, greet and show menu */
        if (cs->step == STEP_MAIN_MENU && msg.text[0] == '\0') {
            send_main_menu(sock, &client_addr);
            continue;
        }

        /* Hand off to the state machine */
        dispatch(sock, cs, msg.text);
    }

    return 0;
}