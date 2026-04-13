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

typedef enum {
    SVR_DISPLAY = 1,
    SVR_PROMPT  = 2,
    CLI_INPUT   = 3
} MsgType;

typedef struct {
    int  type;
    char text[MAX_PAYLOAD];
} Msg;

struct Voter {
    char name[50];
    char regNo[20];
    char password[20];
    int  voted;
};

struct Contestant {
    char name[50];
    char regNo[20];
    char position[30];
    int  votes;
};

struct Admin {
    char name[50];
    char regNo[20];
    char password[20];
};

void handle_client(int sock);
void ensureAdminExists(int sock);
void createAdmin(int sock);
int  verifyAdmin(int sock);
void adminPanel(int sock);
void managePositions(int sock);
void menu(int sock);
void registerVoter(int sock);
void registerContestant(int sock);
void castVote(int sock);
void tallyVotes(int sock);

static void net_print(int sock, const char *fmt, ...) {
    Msg msg;
    va_list args;

    memset(&msg, 0, sizeof(msg));
    msg.type = SVR_DISPLAY;

    va_start(args, fmt);
    vsnprintf(msg.text, MAX_PAYLOAD, fmt, args);
    va_end(args);

    send(sock, &msg, sizeof(msg), 0);
}

static void net_gets(int sock, const char *prompt, char *buf, int size) {
    Msg out, in;

    memset(&out, 0, sizeof(out));
    out.type = SVR_PROMPT;
    strncpy(out.text, prompt, MAX_PAYLOAD - 1);
    send(sock, &out, sizeof(out), 0);

    memset(&in, 0, sizeof(in));
    if(recv(sock, &in, sizeof(in), 0) <= 0) {
        buf[0] = '\0';
        return;
    }

    strncpy(buf, in.text, size - 1);
    buf[size - 1] = '\0';
    buf[strcspn(buf, "\n")] = '\0';   
}


static int net_getint(int sock, const char *prompt) {
    char buf[64];
    net_gets(sock, prompt, buf, sizeof(buf));
    return atoi(buf);
}

static void net_pause(int sock, const char *msg_text) {
    char dummy[8];
    net_gets(sock, msg_text, dummy, sizeof(dummy));
}

void menu(int sock) {
    net_print(sock,
        "\nElectronic Voting System\n"
        "1. Admin Panel\n"
        "2. Register Voter\n"
        "3. Cast Vote\n"
        "4. Exit\n");
}


void ensureAdminExists(int sock) {
    FILE *file;
    char  line[256];

    file = fopen("admin.txt", "r");
    if(file == NULL) {
        net_print(sock, "\nNo admin found. Please create an admin before using the system.\n");
        createAdmin(sock);
        return;
    }

    if(fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        net_print(sock, "\nAdmin file is empty. Please create an admin.\n");
        createAdmin(sock);
        return;
    }

    fclose(file);
}


void createAdmin(int sock) {
    FILE        *file;
    struct Admin a;

    file = fopen("admin.txt", "w");
    if(file == NULL) {
        net_print(sock, "Error creating admin file.\n");
        return;
    }

    net_print(sock, "\n--- Create Admin ---\n");

    net_gets(sock, "Enter Admin Name: ",                a.name,     sizeof(a.name));
    net_gets(sock, "Enter Admin Registration Number: ", a.regNo,    sizeof(a.regNo));
    net_gets(sock, "Enter Admin Password: ",            a.password, sizeof(a.password));

    fprintf(file, "%s|%s|%s\n", a.name, a.regNo, a.password);
    fclose(file);

    net_print(sock, "Admin created successfully.\n");
}

int verifyAdmin(int sock) {
    FILE        *file;
    struct Admin a;
    char         line[256];
    char         inputRegNo[20];
    char         inputPassword[20];
    int          authenticated = 0;

    file = fopen("admin.txt", "r");
    if(file == NULL) {
        net_print(sock, "Admin file not found. Please restart and create an admin.\n");
        return 0;
    }

    net_print(sock, "\n--- Admin Authentication ---\n");
    net_gets(sock, "Enter Admin Registration Number: ", inputRegNo,    sizeof(inputRegNo));
    net_gets(sock, "Enter Admin Password: ",            inputPassword, sizeof(inputPassword));

    while(fgets(line, sizeof(line), file) != NULL) {
        char *token;

        line[strcspn(line, "\n")] = '\0';

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(a.name, token, sizeof(a.name));
        a.name[sizeof(a.name) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(a.regNo, token, sizeof(a.regNo));
        a.regNo[sizeof(a.regNo) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(a.password, token, sizeof(a.password));
        a.password[sizeof(a.password) - 1] = '\0';

        if(strcmp(a.regNo, inputRegNo) == 0 &&
           strcmp(a.password, inputPassword) == 0) {
            authenticated = 1;
            break;
        }
    }

    fclose(file);

    if(!authenticated) {
        net_print(sock, "Invalid admin credentials.\n");
        return 0;
    }

    return 1;
}

void adminPanel(int sock) {
    int choice;

    net_print(sock, "\n--- Admin Panel ---\n");

    if(!verifyAdmin(sock)) {
        net_print(sock, "Admin authentication failed. Returning to main menu.\n");
        return;
    }

    while(1) {
        net_print(sock,
            "\nAdmin Options:\n"
            "1. Manage Positions\n"
            "2. Register Contestant\n"
            "3. Tally Votes\n"
            "4. Back to Main Menu\n");

        choice = net_getint(sock, "Enter choice: ");

        switch(choice) {
            case 1: managePositions(sock);    break;
            case 2: registerContestant(sock); break;
            case 3: tallyVotes(sock);         break;
            case 4: return;
            default: net_print(sock, "Invalid choice.\n");
        }
    }
}

void managePositions(int sock) {
    FILE *file = fopen("positions.txt", "a");
    char  position[30];

    if(file == NULL) {
        net_print(sock, "Error opening positions file.\n");
        return;
    }

    net_print(sock, "\n--- Manage Positions ---\n");
    net_print(sock, "Enter position names to add (empty line to finish):\n");

    while(1) {
        net_gets(sock, "Position: ", position, sizeof(position));

        if(strlen(position) == 0)
            break;

        fprintf(file, "%s\n", position);
    }

    fclose(file);
    net_print(sock, "Positions updated.\n");
}

void registerVoter(int sock) {
    FILE        *file;
    struct Voter v;

    file = fopen("voters.txt", "a");
    if(file == NULL) {
        net_print(sock, "Error opening file\n");
        return;
    }

    net_print(sock, "\n--- Voter Registration ---\n");

    net_gets(sock, "Enter Name: ",                v.name,     sizeof(v.name));
    net_gets(sock, "Enter Registration Number: ", v.regNo,    sizeof(v.regNo));
    net_gets(sock, "Enter Password: ",            v.password, sizeof(v.password));

    v.voted = 0;

    fprintf(file, "%s|%s|%s|%d\n", v.name, v.regNo, v.password, v.voted);
    fclose(file);

    net_print(sock, "Voter registered successfully!\n");
    net_pause(sock, "Press Enter to return to menu...");
}

void registerContestant(int sock) {
    FILE             *file;
    struct Contestant c;
    char              line[256];
    char              positions[100][30];
    int               posCount = 0;

    FILE *pfile = fopen("positions.txt", "r");
    if(pfile == NULL) {
        net_print(sock, "No positions found. Please create positions first.\n");
        return;
    }

    while(fgets(line, sizeof(line), pfile) != NULL && posCount < 100) {
        line[strcspn(line, "\n")] = '\0';
        if(strlen(line) == 0) continue;
        strncpy(positions[posCount], line, sizeof(positions[posCount]));
        positions[posCount][sizeof(positions[posCount]) - 1] = '\0';
        posCount++;
    }
    fclose(pfile);

    if(posCount == 0) {
        net_print(sock, "No positions defined. Please create positions first.\n");
        return;
    }

    file = fopen("contestants.txt", "a");
    if(file == NULL) {
        net_print(sock, "Error opening contestants file\n");
        return;
    }

    net_print(sock, "\n--- Contestant Registration ---\n");

    net_gets(sock, "Enter Name: ",                c.name,  sizeof(c.name));
    net_gets(sock, "Enter Registration Number: ", c.regNo, sizeof(c.regNo));

    net_print(sock, "\nAvailable Positions:\n");
    for(int i = 0; i < posCount; i++) {
        net_print(sock, "%d. %s\n", i + 1, positions[i]);
    }

    int posChoice = net_getint(sock, "Select position number to contest for: ");

    if(posChoice < 1 || posChoice > posCount) {
        net_print(sock, "Invalid position choice.\n");
        fclose(file);
        return;
    }

    strncpy(c.position, positions[posChoice - 1], sizeof(c.position));
    c.position[sizeof(c.position) - 1] = '\0';
    c.votes = 0;

    fprintf(file, "%s|%s|%s|%d\n", c.name, c.regNo, c.position, c.votes);
    fclose(file);

    net_print(sock, "Contestant registered successfully!\n");
    net_pause(sock, "Press Enter to return to menu...");
}

void castVote(int sock) {
    char         regNo[20], password[20];
    int          found = 0, votedAlready = 0;
    struct Voter v;
    char         line[256];

    net_print(sock, "\n--- Cast Vote ---\n");

    net_gets(sock, "Enter your Registration Number: ", regNo,    sizeof(regNo));
    net_gets(sock, "Enter your Password: ",            password, sizeof(password));

    FILE *vfile = fopen("voters.txt", "r");
    if(vfile == NULL) {
        net_print(sock, "Error opening voter file\n");
        return;
    }

    while(fgets(line, sizeof(line), vfile) != NULL) {
        char *token;
        line[strcspn(line, "\n")] = '\0';

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(v.name, token, sizeof(v.name));
        v.name[sizeof(v.name) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(v.regNo, token, sizeof(v.regNo));
        v.regNo[sizeof(v.regNo) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(v.password, token, sizeof(v.password));
        v.password[sizeof(v.password) - 1] = '\0';

        token = strtok(NULL, "|");
        v.voted = token ? atoi(token) : 0;

        if(strcmp(v.regNo, regNo) == 0 &&
           strcmp(v.password, password) == 0) {
            found = 1;
            if(v.voted == 1) votedAlready = 1;
        }
    }
    fclose(vfile);

    if(!found) {
        net_print(sock, "Voter not registered or wrong credentials.\n");
        return;
    }
    if(votedAlready) {
        net_print(sock, "You have already voted.\n");
        return;
    }
    struct Contestant c;
    FILE *cfile = fopen("contestants.txt", "r+");
    if(cfile == NULL) {
        net_print(sock, "No contestants found.\n");
        return;
    }

    char names[100][50];
    char regNos[100][20];
    char pos[100][30];
    int  votes[100];
    int  count = 0;

    while(fgets(line, sizeof(line), cfile) != NULL && count < 100) {
        char *token;
        line[strcspn(line, "\n")] = '\0';

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(c.name, token, sizeof(c.name));
        c.name[sizeof(c.name) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.regNo, token, sizeof(c.regNo));
        c.regNo[sizeof(c.regNo) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.position, token, sizeof(c.position));
        c.position[sizeof(c.position) - 1] = '\0';

        token = strtok(NULL, "|");
        c.votes = token ? atoi(token) : 0;

        strcpy(names[count],  c.name);
        strcpy(regNos[count], c.regNo);
        strcpy(pos[count],    c.position);
        votes[count] = c.votes;
        count++;
    }

    if(count == 0) {
        net_print(sock, "No contestants found.\n");
        fclose(cfile);
        return;
    }
    FILE *pfile = fopen("positions.txt", "r");
    if(pfile == NULL) {
        net_print(sock, "No positions found.\n");
        fclose(cfile);
        return;
    }

    char positions[100][30];
    int  posCount = 0;

    while(fgets(line, sizeof(line), pfile) != NULL && posCount < 100) {
        line[strcspn(line, "\n")] = '\0';
        if(strlen(line) == 0) continue;
        strncpy(positions[posCount], line, sizeof(positions[posCount]));
        positions[posCount][sizeof(positions[posCount]) - 1] = '\0';
        posCount++;
    }
    fclose(pfile);

    if(posCount == 0) {
        net_print(sock, "No positions defined.\n");
        fclose(cfile);
        return;
    }
    for(int p = 0; p < posCount; p++) {
        int candidateIdx[100];
        int candidateCount = 0;

        net_print(sock, "\nPosition: %s\n", positions[p]);

        for(int j = 0; j < count; j++) {
            if(strcmp(pos[j], positions[p]) == 0) {
                candidateIdx[candidateCount++] = j;
            }
        }

        if(candidateCount == 0) {
            net_print(sock, "No contestants available for this position.\n");
            net_pause(sock, "Press Enter to continue to the next position...");
            continue;
        }

        for(int k = 0; k < candidateCount; k++) {
            int idx = candidateIdx[k];
            net_print(sock, "%d. %s (%s)\n", k + 1, names[idx], regNos[idx]);
        }

        int choice = net_getint(sock,
            "Enter candidate number to vote for (or 0 to skip this position): ");

        if(choice == 0) {
            /* skip */
        } else if(choice >= 1 && choice <= candidateCount) {
            votes[candidateIdx[choice - 1]]++;
        } else {
            net_print(sock, "Invalid choice for this position. Skipping.\n");
        }
    }
    freopen("contestants.txt", "w", cfile);
    for(int j = 0; j < count; j++) {
        fprintf(cfile, "%s|%s|%s|%d\n", names[j], regNos[j], pos[j], votes[j]);
    }
    fclose(cfile);

    FILE *tempV;
    vfile = fopen("voters.txt", "r");
    tempV = fopen("voters_temp.txt", "w");

    if(vfile == NULL || tempV == NULL) {
        net_print(sock, "Error updating voter status, but vote was recorded.\n");
        if(vfile) fclose(vfile);
        if(tempV) fclose(tempV);
        return;
    }

    while(fgets(line, sizeof(line), vfile) != NULL) {
        char *token;
        line[strcspn(line, "\n")] = '\0';

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(v.name, token, sizeof(v.name));
        v.name[sizeof(v.name) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(v.regNo, token, sizeof(v.regNo));
        v.regNo[sizeof(v.regNo) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(v.password, token, sizeof(v.password));
        v.password[sizeof(v.password) - 1] = '\0';

        token = strtok(NULL, "|");
        v.voted = token ? atoi(token) : 0;

        if(strcmp(v.regNo, regNo) == 0 &&
           strcmp(v.password, password) == 0) {
            v.voted = 1;
        }

        fprintf(tempV, "%s|%s|%s|%d\n", v.name, v.regNo, v.password, v.voted);
    }

    fclose(vfile);
    fclose(tempV);

    remove("voters.txt");
    rename("voters_temp.txt", "voters.txt");

    net_print(sock, "Vote cast successfully!\n");
}


void tallyVotes(int sock) {
    struct Contestant c;
    FILE             *file;
    int               maxVotes = 0;
    char              line[256];

    file = fopen("contestants.txt", "r");
    if(file == NULL) {
        net_print(sock, "No contestants found.\n");
        return;
    }

    net_print(sock, "\n--- Election Results ---\n");

    while(fgets(line, sizeof(line), file) != NULL) {
        char *token;
        line[strcspn(line, "\n")] = '\0';

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(c.name, token, sizeof(c.name));
        c.name[sizeof(c.name) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.regNo, token, sizeof(c.regNo));
        c.regNo[sizeof(c.regNo) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.position, token, sizeof(c.position));
        c.position[sizeof(c.position) - 1] = '\0';

        token = strtok(NULL, "|");
        c.votes = token ? atoi(token) : 0;

        if(c.votes > maxVotes) maxVotes = c.votes;
    }

    fseek(file, 0, SEEK_SET);
    net_print(sock, "\nVote Counts:\n");

    while(fgets(line, sizeof(line), file) != NULL) {
        char *token;
        line[strcspn(line, "\n")] = '\0';

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(c.name, token, sizeof(c.name));
        c.name[sizeof(c.name) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.regNo, token, sizeof(c.regNo));
        c.regNo[sizeof(c.regNo) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.position, token, sizeof(c.position));
        c.position[sizeof(c.position) - 1] = '\0';

        token = strtok(NULL, "|");
        c.votes = token ? atoi(token) : 0;

        net_print(sock, "%s (%s) for %s - %d votes\n",
                  c.name, c.regNo, c.position, c.votes);
    }

    fseek(file, 0, SEEK_SET);
    net_print(sock, "\nWinner(s):\n");

    while(fgets(line, sizeof(line), file) != NULL) {
        char *token;
        line[strcspn(line, "\n")] = '\0';

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(c.name, token, sizeof(c.name));
        c.name[sizeof(c.name) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.regNo, token, sizeof(c.regNo));
        c.regNo[sizeof(c.regNo) - 1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.position, token, sizeof(c.position));
        c.position[sizeof(c.position) - 1] = '\0';

        token = strtok(NULL, "|");
        c.votes = token ? atoi(token) : 0;

        if(c.votes == maxVotes) {
            net_print(sock, "%s (%s) for %s\n", c.name, c.regNo, c.position);
        }
    }

    fclose(file);
    net_pause(sock, "\nPress Enter to return to menu...");
}

void handle_client(int sock) {
    int choice;

    ensureAdminExists(sock);

    while(1) {
        menu(sock);
        choice = net_getint(sock, "Enter choice: ");

        switch(choice) {
            case 1: adminPanel(sock);    break;
            case 2: registerVoter(sock); break;
            case 3: castVote(sock);      break;
            case 4:
                net_print(sock, "Goodbye!\n");
                return;
            default:
                net_print(sock, "Invalid choice\n");
        }
    }
}

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        perror("socket");
        exit(1);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if(bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if(listen(server_fd, 1) < 0) {
        perror("listen");
        exit(1);
    }

    printf("[server] Voting server listening on port %d ...\n", PORT);

    while(1) {
        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr, &client_len);
        if(client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("[server] Client connected.\n");
        handle_client(client_fd);
        close(client_fd);
        printf("[server] Client disconnected.\n");
    }

    close(server_fd);
    return 0;
}