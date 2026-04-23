#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#define PORT 9090
#define BUFFER_SIZE 2048

static int g_sock = -1;
static struct sockaddr_in g_server_addr;
static socklen_t g_addr_len = sizeof(g_server_addr);

int send_request(const char *request, char *response, size_t response_size);
int verifyAdmin(void);
int getInput(const char *prompt, char *buffer, size_t size);
void adminPanel(void);
void managePositions(void);
void registerContestant(void);
void tallyVotes(void);
void displayAdminInfo(void);
void castVote(void);
void viewContestants(void);
void ensureAdminExists(void);

int send_request(const char *request, char *response, size_t response_size)
{
    if (sendto(g_sock, request, strlen(request) + 1, 0,
               (struct sockaddr *)&g_server_addr, g_addr_len) < 0) {
        return 0;
    }

    memset(response, 0, response_size);
    if (recvfrom(g_sock, response, response_size, 0,
                 (struct sockaddr *)&g_server_addr, &g_addr_len) <= 0) {
        return 0;
    }

    return 1;
}

int main(void)
{
    char server_ip[64];

    printf("Enter server IP address: ");
    fflush(stdout);

    if (fgets(server_ip, sizeof(server_ip), stdin) == NULL) {
        perror("Input error");
        exit(1);
    }
    server_ip[strcspn(server_ip, "\n")] = '\0';

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock <= 0) {
        perror("socket");
        exit(1);
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&g_server_addr, 0, sizeof(g_server_addr));
    g_server_addr.sin_family = AF_INET;
    g_server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &g_server_addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        close(g_sock);
        return 1;
    }

    printf("\nServer address %s:%d  initialised\n", server_ip, PORT);

    ensureAdminExists();

    while (1) {
        char choice[20];

        printf("\n=== Main Menu ===\n");
        printf("1. Admin Panel\n");
        printf("2. Register Voter\n");
        printf("3. Cast Vote\n");
        printf("4. Exit\n");
        printf("Enter choice: ");
        fflush(stdout);

        if (!fgets(choice, sizeof(choice), stdin)) {
            break;
        }
        choice[strcspn(choice, "\n")] = '\0';

        if (strcmp(choice, "1") == 0) {
            adminPanel();
        } else if (strcmp(choice, "2") == 0) {
            char name[50], regNo[20], password[20];
            char request[BUFFER_SIZE], response[BUFFER_SIZE];

            if (!getInput("\nVoter name: ", name, sizeof(name))) continue;
            if (!getInput("Registration number: ", regNo, sizeof(regNo))) continue;
            if (!getInput("Password: ", password, sizeof(password))) continue;

            if (strlen(name) == 0 || strlen(regNo) == 0 || strlen(password) == 0) {
                printf("Error: Empty fields\n");
                continue;
            }

            if (strlen(password) < 4) {
                printf("Password too short\n");
                continue;
            }

            snprintf(request, sizeof(request), "REGISTER|%s|%s|%s", name, regNo, password);
            if (send_request(request, response, sizeof(response))) {
                printf("\nServer Response: %s\n", response);
            } else {
                printf("No response from server.\n");
            }
        } else if (strcmp(choice, "3") == 0) {
            castVote();
        } else if (strcmp(choice, "4") == 0) {
            printf("Goodbye!\n");
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    close(g_sock);
    return 0;
}

int getInput(const char *prompt, char *buffer, size_t size) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (!fgets(buffer, size, stdin)) {
        return 0;
    }

    buffer[strcspn(buffer, "\n")] = '\0';
    
    return 1;
}


int verifyAdmin(void)
{
    char regNo[20], password[20];
    char request[BUFFER_SIZE], response[BUFFER_SIZE];

    printf("\n--- Admin Authentication ---\n");
    if (!getInput("Enter Admin Registration Number: ", regNo, sizeof(regNo)))
        return 0;

    if (!getInput("Enter Admin Password: ", password, sizeof(password)))
        return 0;

    snprintf(request, sizeof(request), "VERIFY_ADMIN|%s|%s", regNo, password);
    if (!send_request(request, response, sizeof(response))) {
        printf("No response from server.\n");
        return 0;
    }

    if (strncmp(response, "SUCCESS", 7) == 0) {
        return 1;
    }

    printf("%s\n", response);
    return 0;
}

void displayAdminInfo(void)
{
    char regNo[20], password[20];
    char request[BUFFER_SIZE], response[BUFFER_SIZE];

    printf("\n--- View Admin Info ---\n");

    if (!getInput("Enter your Admin Reg No: ", regNo, sizeof(regNo)))
        return;

    if (!getInput("Enter your Admin Password: ", password, sizeof(password)))
        return;

    snprintf(request, sizeof(request), "GET_ADMIN_INFO|%s|%s", regNo, password);
    if (send_request(request, response, sizeof(response))) {
        printf("\n%s\n", response);
    } else {
        printf("No response from server.\n");
    }
}

void adminPanel(void)
{
    char choice[20];

    printf("\n--- Admin Panel ---\n");

    if (!verifyAdmin()) {
        printf("Admin authentication failed. Returning to main menu.\n");
        return;
    }

    while (1) {
        printf("\nAdmin Options:\n");
        printf("1. Manage Positions\n");
        printf("2. Register Contestant\n");
        printf("3. View Contestants\n");
        printf("4. Tally Votes\n");
        printf("5. View Admin Info\n");
        printf("6. Back to Main Menu\n");
        printf("Enter choice: ");

        if (!fgets(choice, sizeof(choice), stdin)) return;
        choice[strcspn(choice, "\n")] = '\0';

        if (strcmp(choice, "1") == 0) {
            managePositions();
        } else if (strcmp(choice, "2") == 0) {
            registerContestant();
        } else if (strcmp(choice, "3") == 0) {
            viewContestants();
        } else if (strcmp(choice, "4") == 0) {
            tallyVotes();
        } else if (strcmp(choice, "5") == 0) {
            displayAdminInfo();
        } else if (strcmp(choice, "6") == 0) {
            return;
        } else {
            printf("Invalid choice.\n");
        }
    }
}

void managePositions(void)
{
    char position[30];
    char regNo[20], password[20];
    char request[BUFFER_SIZE], response[BUFFER_SIZE];

    printf("\n--- Manage Positions ---\n");
    if (!getInput("Enter your Admin Reg No: ", regNo, sizeof(regNo)))
        return;

    if (!getInput("Enter your Admin Password: ", password, sizeof(password)))
        return;

    printf("Enter position names to add (empty line to finish):\n");
    while (1) {
        if (!getInput("Position: ", position, sizeof(position)))
            break;

        if (position[0] == '\0')
            break;

        snprintf(request, sizeof(request), "ADD_POSITION|%s|%s|%s", regNo, password, position);
        if (send_request(request, response, sizeof(response))) {
            printf("Server Response: %s\n", response);
        } else {
            printf("No response from server.\n");
            break;
        }
    }
}

void registerContestant(void)
{
    char name[50], contestantRegNo[20], position[30];
    char regNo[20], password[20];
    char request[BUFFER_SIZE], response[BUFFER_SIZE];

    printf("\n--- Contestant Registration ---\n");

    if (!getInput("Enter Name: ", name, sizeof(name)))
        return;
    if (!getInput("Enter Reg No: ", contestantRegNo, sizeof(contestantRegNo)))
        return;
    if (!getInput("Enter Position: ", position, sizeof(position)))
        return;
    if (!getInput("Enter your Admin Reg No: ", regNo, sizeof(regNo)))
        return;
    if (!getInput("Enter your Admin Password: ", password, sizeof(password)))
        return;

    snprintf(request, sizeof(request), "REGISTER_CONTESTANT|%s|%s|%s|%s|%s",
             regNo, password, name, contestantRegNo, position);

    if (send_request(request, response, sizeof(response))) {
        printf("\nServer Response: %s\n", response);
    } else {
        printf("No response from server.\n");
    }
}

void tallyVotes(void)
{
    char regNo[20], password[20];
    char request[BUFFER_SIZE], response[BUFFER_SIZE];

    printf("\n--- Election Results ---\n");

    if (!getInput("Enter your Admin Reg No: ", regNo, sizeof(regNo)))
        return;
    if (!getInput("Enter your Admin Password: ", password, sizeof(password)))
        return;

    snprintf(request, sizeof(request), "TALLY_VOTES|%s|%s", regNo, password);
    if (send_request(request, response, sizeof(response))) {
        printf("\n%s\n", response);
    } else {
        printf("No response from server.\n");
    }
}

void viewContestants(void)
{
    char regNo[20], password[20];
    char request[BUFFER_SIZE], response[BUFFER_SIZE];

    printf("\n--- View Contestants ---\n");

    if (!getInput("Enter your Admin Reg No: ", regNo, sizeof(regNo)))
        return;
    if (!getInput("Enter your Admin Password: ", password, sizeof(password)))
        return;

    snprintf(request, sizeof(request), "VIEW_CONTESTANTS|%s|%s", regNo, password);
    if (send_request(request, response, sizeof(response))) {
        printf("\n%s\n", response);
    } else {
        printf("No response from server.\n");
    }
}

static int get_positions(char positions[100][30])
{
    char request[BUFFER_SIZE], response[BUFFER_SIZE];
    snprintf(request, sizeof(request), "GET_POSITIONS");
    if (!send_request(request, response, sizeof(response))) {
        return 0;
    }

    if (strncmp(response, "ERROR", 5) == 0) return 0;

    int count = 0;
    char *pos_copy = strdup(response);
    char *pos = strtok(pos_copy, "\n");
    while (pos && count < 100) {
        strncpy(positions[count], pos, 29);
        positions[count][29] = '\0';
        count++;
        pos = strtok(NULL, "\n");
    }
    free(pos_copy);
    return count;
}

void castVote(void)
{
    char voter_regNo[20], voter_pwd[20], contestant_regNo[20];
    char request[BUFFER_SIZE], response[BUFFER_SIZE];
    char avail_pos[100][30];
    int pos_count;

    printf("\n=== Cast Your Vote ===\n");

    if (!getInput("Your Registration Number: ", voter_regNo, sizeof(voter_regNo)))
        return;
    if (!getInput("Your Password: ", voter_pwd, sizeof(voter_pwd)))
        return;

    pos_count = get_positions(avail_pos);
    if (pos_count == 0) {
        printf("No positions available for voting.\n");
        return;
    }

    for (int i = 0; i < pos_count; i++) {
        printf("\n--- Voting for Position: %s ---\n", avail_pos[i]);

        snprintf(request, sizeof(request), "VIEW_CONTESTANTS|%s", avail_pos[i]);
        if (send_request(request, response, sizeof(response))) {
            printf("%s\n", response);
        }

        if (strstr(response, "No contestants") != NULL) {
            continue;
        }

        if (!getInput("Enter Registration Number of candidate (or press Enter to skip): ", contestant_regNo, sizeof(contestant_regNo)))
            break;

        if (strlen(contestant_regNo) == 0) continue;

        snprintf(request, sizeof(request), "CAST_VOTE|%s|%s|%s",
                 voter_regNo, voter_pwd, contestant_regNo);

        if (send_request(request, response, sizeof(response))) {
            printf("Result: %s\n", response);
            if (strstr(response, "already completed") != NULL) {
                return;
            }
        } else {
            printf("No response from server.\n");
            break;
        }
    }

    /* Finalize voting process for this voter */
    snprintf(request, sizeof(request), "MARK_VOTED|%s", voter_regNo);
    if (send_request(request, response, sizeof(response))) {
        printf("\nVoting process complete. Thank you!\n");
    } else {
        printf("\nError finalising vote status.\n");
    }
}

void ensureAdminExists(void)
{
    char request[BUFFER_SIZE], response[BUFFER_SIZE];

    snprintf(request, sizeof(request), "CHECK_ADMIN");
    if (!send_request(request, response, sizeof(response))) {
        return;
    }

    if (strcmp(response, "NOT_EXISTS") == 0) {
        printf("\n=== Admin Setup Required ===\n");
        char name[50], regNo[20], password[20];

        printf("Admin does not exist. Please create one.\n");
        if (!getInput("Admin Name: ", name, sizeof(name)))
            return;
        if (!getInput("Admin Reg No: ", regNo, sizeof(regNo)))
            return;
        if (!getInput("Admin Password: ", password, sizeof(password)))
            return;

        snprintf(request, sizeof(request), "CREATE_ADMIN|%s|%s|%s",
                 name, regNo, password);
        if (send_request(request, response, sizeof(response))) {
            printf("\n%s\n", response);
        }
    }
}