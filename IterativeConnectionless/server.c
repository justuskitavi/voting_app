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

/* Message Types (old prompt-based model, no longer used in requests) */
typedef enum { SVR_DISPLAY = 1, SVR_PROMPT = 2, CLI_INPUT = 3 } MsgType;

/* Request and Response Types (new client-driven model) */
typedef enum {
    REQ_REGISTER_VOTER = 10,
    REQ_VERIFY_ADMIN = 20,
    REQ_CAST_VOTE = 30
} RequestType;

typedef enum {
    RESP_SUCCESS = 1,
    RESP_ERROR = 2,
    RESP_DATA = 3
} ResponseType;

/* Extended message structure to support both old and new protocols */
typedef struct {
    int  type;
    int  req_type;       /* For CLIENT_REQUEST messages */
    int  resp_status;    /* For SVR_RESPONSE: RESP_SUCCESS/RESP_ERROR/RESP_DATA */
    char text[MAX_PAYLOAD];
    
    /* Request payload fields (for client-driven requests) */
    struct {
        char name[50];
        char regNo[20];
        char password[20];
    } voter_data;
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

/* ================================================================== */
/*  Response helper — send a response message back to client           */
/* ================================================================== */
static void net_send_response(int sock,
                              const struct sockaddr_in *caddr,
                              int resp_status,
                              const char *message)
{
    Msg m;
    memset(&m, 0, sizeof(m));
    m.type = 99;  /* Mark as response (new protocol) */
    m.resp_status = resp_status;
    strncpy(m.text, message, MAX_PAYLOAD - 1);
    sendto(sock, &m, sizeof(m), 0,
           (const struct sockaddr *)caddr, sizeof(*caddr));
}

/* ================================================================== */
/*  Request handler — processes CLIENT_REQUEST and sends response     */
/* ================================================================== */
static void handle_request(int sock,
                           ClientSession *cs,
                           const Msg *req)
{
    const struct sockaddr_in *addr = &cs->addr;
    
    switch (req->req_type) {
    
    case REQ_REGISTER_VOTER: {
        /* Verify non-empty fields */
        if (strlen(req->voter_data.name) == 0 ||
            strlen(req->voter_data.regNo) == 0 ||
            strlen(req->voter_data.password) == 0) {
            net_send_response(sock, addr, RESP_ERROR,
                "Error: Empty fields in registration.");
            return;
        }
        
        /* TODO: Check if voter already exists in voters.txt */
        
        /* Save voter to file */
        save_voter(req->voter_data.name, req->voter_data.regNo,
                   req->voter_data.password);
        
        net_send_response(sock, addr, RESP_SUCCESS,
            "Voter registered successfully!");
        break;
    }
    
    case REQ_VERIFY_ADMIN: {
        if (verify_admin(req->voter_data.regNo, req->voter_data.password)) {
            net_send_response(sock, addr, RESP_SUCCESS,
                "Admin authenticated.");
        } else {
            net_send_response(sock, addr, RESP_ERROR,
                "Invalid admin credentials.");
        }
        break;
    }
    
    case REQ_CAST_VOTE: {
        /* TODO: Implement vote casting logic */
        net_send_response(sock, addr, RESP_SUCCESS,
            "Vote recorded successfully!");
        break;
    }
    
    default:
        net_send_response(sock, addr, RESP_ERROR,
            "Unknown request type.");
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
            printf("[udp-server] Datagram from (%s:%d)  req_type=%d\n",
                   client_ip, client_port, msg.req_type);

        /* Handle incoming request (no more state machine, no more prompts from server) */
        if (msg.req_type > 0) {
            handle_request(sock, cs, &msg);
        } else if (msg.text[0] == '\0') {
            /* First connection: client sent empty message; acknowledge but no prompt */
            net_send_response(sock, &client_addr, RESP_SUCCESS,
                "Connected to voting server. Use client menu to proceed.");
        }
    }

    return 0;
}