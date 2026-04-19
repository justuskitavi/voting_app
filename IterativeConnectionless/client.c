/* =========================================================
 * client_udp_fixed.c  —  PURE UDP (connectionless) client
 * ========================================================= */

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

void ensureAdminExists();
int verifyAdmin();
void createAdmin();
void adminPanel();
void managePositions();
void registerVoter();
void registerContestant();
void castVote();
void tallyVotes();
void displayAdminInfo();

struct Voter {
    char name[50];
    char regNo[20];
    char password[20];
    int voted;
};

struct Contestant {
    char name[50];
    char regNo[20];
    char position[30];
    int votes;
};

struct Admin {
    char name[50];
    char regNo[20];
    char password[20];
};


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

    /* ✅ Create UDP socket */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    /* ✅ Set timeout (prevents infinite blocking) */
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        exit(1);
    }

    printf("\n[UDP Client] Connected to %s:%d\n", server_ip, PORT);

    /* ===================================================== */
    /* MAIN LOOP */
    /* ===================================================== */

    int running = 1;

    // while (running) {
    //     printf("\n=== Main Menu ===\n");
    //     printf("1. Admin Panel\n");
    //     printf("2. Register Voter\n");
    //     printf("3. Exit\n");
    //     printf("Enter choice: ");
    //     fflush(stdout);

    //     char choice[20];
    //     fgets(choice, sizeof(choice), stdin);
    //     choice[strcspn(choice, "\n")] = '\0';

    //     ensureAdminExists();

    //     if (strcmp(choice, "1") == 0) {
    //         /* ---------------- ADMIN ---------------- */
    //         char regNo[20], password[20];

    //         printf("\nAdmin Reg No: ");
    //         fgets(regNo, sizeof(regNo), stdin);
    //         regNo[strcspn(regNo, "\n")] = '\0';

    //         printf("Admin Password: ");
    //         fgets(password, sizeof(password), stdin);
    //         password[strcspn(password, "\n")] = '\0';

    //         if (strlen(regNo) == 0 || strlen(password) == 0) {
    //             printf("Error: Empty fields\n");
    //             continue;
    //         }

    //         /* ✅ Build message safely */
    //         char buffer[BUFFER_SIZE];
    //         snprintf(buffer, sizeof(buffer),
    //                  "VERIFY_ADMIN|%s|%s",
    //                  regNo, password);

    //         /* ✅ sendto() */
    //         sendto(sock, buffer, strlen(buffer) + 1, 0,
    //                (struct sockaddr *)&server_addr, addr_len);

    //         /* ✅ receive */
    //         memset(buffer, 0, sizeof(buffer));
    //         if (recvfrom(sock, buffer, sizeof(buffer), 0,
    //                      (struct sockaddr *)&server_addr, &addr_len) > 0) {
    //             printf("\nServer Response: %s\n", buffer);
    //         } else {
    //             printf("No response from server.\n");
    //         }

    //     } else if (strcmp(choice, "2") == 0) {
    //         /* ---------------- REGISTER ---------------- */
    //         char name[50], regNo[20], password[20];

    //         printf("\nVoter Name: ");
    //         fgets(name, sizeof(name), stdin);
    //         name[strcspn(name, "\n")] = '\0';

    //         printf("Registration Number: ");
    //         fgets(regNo, sizeof(regNo), stdin);
    //         regNo[strcspn(regNo, "\n")] = '\0';

    //         printf("Password: ");
    //         fgets(password, sizeof(password), stdin);
    //         password[strcspn(password, "\n")] = '\0';

    //         if (strlen(name) == 0 || strlen(regNo) == 0 || strlen(password) == 0) {
    //             printf("Error: Empty fields\n");
    //             continue;
    //         }

    //         if (strlen(password) < 4) {
    //             printf("Password too short\n");
    //             continue;
    //         }

    //         /* ✅ Build message */
    //         char buffer[BUFFER_SIZE];
    //         snprintf(buffer, sizeof(buffer),
    //                  "REGISTER|%s|%s|%s",
    //                  name, regNo, password);

    //         sendto(sock, buffer, strlen(buffer) + 1, 0,
    //                (struct sockaddr *)&server_addr, addr_len);

    //         /* ✅ receive */
    //         memset(buffer, 0, sizeof(buffer));
    //         if (recvfrom(sock, buffer, sizeof(buffer), 0,
    //                      (struct sockaddr *)&server_addr, &addr_len) > 0) {
    //             printf("\nServer Response: %s\n", buffer);
    //         } else {
    //             printf("No response from server.\n");
    //         }

    //     } else if (strcmp(choice, "3") == 0) {
    //         printf("Goodbye!\n");
    //         running = 0;
    //     } else {
    //         printf("Invalid choice.\n");
    //     }
    // }
    while (running) {
        printf("\n=== Main Menu ===\n");
        printf("1. Admin Panel\n");
        printf("2. Register Voter\n");
        printf("3. Exit\n");
        printf("Enter choice: ");
        fflush(stdout);

        char choice[20];
        fgets(choice, sizeof(choice), stdin);
        choice[strcspn(choice, "\n")] = '\0';

        ensureAdminExists();

        if (strcmp(choice, "1") == 0) {
            adminPanel();
        }else if(strcmp(choice, "2") == 0) {
            char name[50], regNo[20], password[20];

            printf("\n Voter name: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0';

            printf("Registration number: ");
            fgets(regNo, sizeof(regNo), stdin);
            regNo[strcspn(regNo, "\n")] = '\0';

            printf("\n Password: ");
            fgets(password, sizeof(password), stdin);
            password[strcspn(password, "\n")] = '\0';

            if (strlen(name) == 0 || strlen(regNo) == 0 || strlen(password) == 0) {
                printf("Error: Empty fields\n");
                continue;
            }

            if(strlen(password) < 4) {
                printf("Password too short");
                continue;
            }

            /* Send REGISTER message over UDP */

            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "REGISTER|%s|%s|%s", name, regNo, password);
            sendto(sock, buffer, strlen(buffer) + 1, 0, 
                    (struct sockaddr *)&server_addr, addr_len);
            memset(buffer, 0, sizeof(buffer));
            if(recvfrom(sock, buffer, sizeof(buffer), 0, 
                    (struct sockaddr *)&server_addr, &addr_len) > 0) {
                printf("\nServer Response: %s\n", buffer);
            } else {
                printf("No response from server.\n");
            }

        }else if (strcmp(choice, "3") == 0){
            printf("Goodbye!\n");
            running = 0;
        }else {
            printf("Invalid choice.\n");
        }

    }

    close(sock);
    return 0;
}

void ensureAdminExists() {
    FILE *file;
    char line[256];

    file = fopen("admin.txt", "r");
    if (file == NULL){
        printf("\n No admin found. Please create an admin before using the system.\n");
        createAdmin();
        return;
    }

    if(fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        printf("\n Admin file is empty. Please create an admin.\n");
        createAdmin();
        return;
    }
    fclose(file);

}

void createAdmin() {
    FILE *file;
    struct Admin a;

    file = fopen("admin.txt", "w");
    if(file == NULL) {
        printf("\n Error creating admin file.\n");
        exit(1);
    }

    printf("\n--- Create Admin ---\n");

    printf("Enter Admin Name: ");
    fgets(a.name, sizeof(a.name), stdin);
    a.name[strcspn(a.name, "\n")] = 0;

    printf("Enter Admin Registration Number: ");
    fgets(a.regNo, sizeof(a.regNo), stdin);
    a.regNo[strcspn(a.regNo, "\n")] = 0;

    printf("Enter Admin Password: ");
    fgets(a.password, sizeof(a.password), stdin);
    a.password[strcspn(a.password, "\n")] = 0;

    fprintf(file, "%s|%s|%s\n", a.name, a.regNo, a.password);
    fclose(file);

    printf("Admin created successfully.\n");
}

int verifyAdmin() {
    FILE *file;
    struct Admin a;
    char line[256];
    char inputRegNo[20];
    char inputPassword[20];
    int authenticated = 0;

    file = fopen("admin.txt", "r");
    if(file == NULL) {
        printf("Admin file not found. Please restart and create an admin.\n");
        return 0;
    }

    printf("\n--- Admin Authentication ---\n");
    printf("Enter Admin Registration Number: ");
    fgets(inputRegNo, sizeof(inputRegNo), stdin);
    inputRegNo[strcspn(inputRegNo, "\n")] = 0;

    printf("Enter Admin Password: ");
    fgets(inputPassword, sizeof(inputPassword), stdin);
    inputPassword[strcspn(inputPassword, "\n")] = 0;

    while(fgets(line, sizeof(line), file) != NULL) {
        char *token;

        line[strcspn(line, "\n")] = 0;

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(a.name, token, sizeof(a.name));
        a.name[sizeof(a.name)-1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(a.regNo, token, sizeof(a.regNo));
        a.regNo[sizeof(a.regNo)-1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(a.password, token, sizeof(a.password));
        a.password[sizeof(a.password)-1] = '\0';

        if(strcmp(a.regNo, inputRegNo) == 0 && strcmp(a.password, inputPassword) == 0) {
            authenticated = 1;
            break;
        }
    }

    fclose(file);

    if(!authenticated) {
        printf("Invalid admin credentials.\n");
        return 0;
    }

    return 1;
}

void displayAdminInfo() {
    char regNo[20], password[20];

    printf("\n--- View Admin Info ---\n");

    printf("Enter your Admin Reg No: ");
    getchar();
    fgets(regNo, sizeof(regNo), stdin);
    regNo[strcspn(regNo, "\n")] = 0;

    printf("Enter your Admin Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    // Send to server
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "GET_ADMIN_INFO|%s|%s", regNo, password);

    struct sockaddr_in server_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    socklen_t addr_len = sizeof(server_addr);
    sendto(sock, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)&server_addr, addr_len);

    memset(buffer, 0, sizeof(buffer));
    if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len) > 0) {
        printf("\n%s\n", buffer);
    } else {
        printf("No response from server.\n");
    }
    close(sock);

    printf("Press Enter to return to menu...");
    getchar();
}

void adminPanel() {
    int choice;

    printf("\n--- Admin Panel ---\n");

    // Authenticate once before showing admin actions
    if(!verifyAdmin()) {
        printf("Admin authentication failed. Returning to main menu.\n");
        return;
    }

    while(1) {
        printf("\nAdmin Options:\n");
        printf("1. Manage Positions\n");
        printf("2. Register Contestant\n");
        printf("3. Tally Votes\n");
        printf("4. View Admin Info\n");
        printf("5. Back to Main Menu\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        switch(choice) {
            case 1:
            managePositions();
                break;
            case 2:
            registerContestant();            
                break;
            case 3:
            tallyVotes();                
                break;
            case 4:
            displayAdminInfo();
                break;
            case 5:
                return;
            default:
                printf("Invalid choice.\n");
        }
    }
}

void managePositions() {
    char position[30];

    printf("\n--- Manage Positions ---\n");
    printf("Enter position names to add (empty line to finish):\n");

    int ch;
    while((ch = getchar()) != '\n' && ch != EOF) { }

    while(1) {
        printf("Position: ");
        if(fgets(position, sizeof(position), stdin) == NULL) {
            break;
        }
        // If just newline, stop
        if(strcmp(position, "\n") == 0) {
            break;
        }
        position[strcspn(position, "\n")] = 0;
        if(strlen(position) == 0) {
            break;
        }

        // Send to server
        char buffer[BUFFER_SIZE];
        char regNo[20], password[20];

        printf("Enter your Admin Reg No: ");
        fgets(regNo, sizeof(regNo), stdin);
        regNo[strcspn(regNo, "\n")] = 0;

        printf("Enter your Admin Password: ");
        fgets(password, sizeof(password), stdin);
        password[strcspn(password, "\n")] = 0;

        snprintf(buffer, sizeof(buffer), "ADD_POSITION|%s|%s|%s", regNo, password, position);
        
        struct sockaddr_in server_addr;
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
        
        socklen_t addr_len = sizeof(server_addr);
        sendto(sock, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)&server_addr, addr_len);

        memset(buffer, 0, sizeof(buffer));
        if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len) > 0) {
            printf("Server Response: %s\n", buffer);
        } else {
            printf("No response from server.\n");
        }
        close(sock);
    }

    printf("Positions updated.\n");
}
void menu() {
    printf("\nElectronic Voting System\n");
    printf("1. Admin Panel\n");
    printf("2. Register Voter\n");
    printf("3. Cast Vote\n");
    printf("4. Exit\n");
}

void registerVoter() {

    FILE *file;
    struct Voter v;
    char line[256];

    file = fopen("voters.txt", "a");
    if(file == NULL) {
        printf("Error opening file\n");
        return;
    }

    printf("\n--- Voter Registration ---\n");

    printf("Enter Name: ");
    getchar();
    fgets(v.name, sizeof(v.name), stdin);
    v.name[strcspn(v.name, "\n")] = 0; 

    printf("Enter Registration Number: ");
    fgets(v.regNo, sizeof(v.regNo), stdin);
    v.regNo[strcspn(v.regNo, "\n")] = 0;

    printf("Enter Password: ");
    fgets(v.password, sizeof(v.password), stdin);
    v.password[strcspn(v.password, "\n")] = 0;

    v.voted = 0;

    // Use '|' as a delimiter so names can contain spaces
    fprintf(file, "%s|%s|%s|%d\n", v.name, v.regNo, v.password, v.voted);

    fclose(file);

    printf("Voter registered successfully!\n");
    printf("Press Enter to return to menu...");
    getchar();
}

void registerContestant() {
    struct Contestant c;
    char line[256];

    printf("\n--- Contestant Registration ---\n");

    printf("Enter Name: ");
    getchar(); 
    fgets(c.name, sizeof(c.name), stdin);
    c.name[strcspn(c.name, "\n")] = 0; 

    printf("Enter Registration Number: ");
    fgets(c.regNo, sizeof(c.regNo), stdin);
    c.regNo[strcspn(c.regNo, "\n")] = 0;

    printf("Enter Position: ");
    fgets(c.position, sizeof(c.position), stdin);
    c.position[strcspn(c.position, "\n")] = 0;

    char regNo[20], password[20];
    printf("Enter your Admin Reg No: ");
    fgets(regNo, sizeof(regNo), stdin);
    regNo[strcspn(regNo, "\n")] = 0;

    printf("Enter your Admin Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    // Send to server
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "REGISTER_CONTESTANT|%s|%s|%s|%s|%s", 
             regNo, password, c.name, c.regNo, c.position);

    struct sockaddr_in server_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    socklen_t addr_len = sizeof(server_addr);
    sendto(sock, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)&server_addr, addr_len);

    memset(buffer, 0, sizeof(buffer));
    if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len) > 0) {
        printf("\nServer Response: %s\n", buffer);
    } else {
        printf("No response from server.\n");
    }
    close(sock);

    printf("Contestant registered!\n");
    printf("Press Enter to return to menu...");
    getchar();
}

void castVote() {

    char regNo[20], password[20];
    int found = 0, votedAlready = 0;

    printf("\n--- Cast Vote ---\n");

    printf("Enter your Registration Number: ");
    getchar();
    fgets(regNo, sizeof(regNo), stdin);
    regNo[strcspn(regNo, "\n")] = 0;

    printf("Enter your Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    // Phase 1: verify credentials and vote status without modifying file
    FILE *vfile;
    struct Voter v;
    char line[256];
    vfile = fopen("voters.txt", "r");

    if(vfile == NULL) {
        printf("Error opening voter file\n");
        return;
    }

    // Just check credentials and whether this voter has already voted
    while(fgets(line, sizeof(line), vfile) != NULL) {
        char *token;

        line[strcspn(line, "\n")] = 0;

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(v.name, token, sizeof(v.name));
        v.name[sizeof(v.name)-1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(v.regNo, token, sizeof(v.regNo));
        v.regNo[sizeof(v.regNo)-1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(v.password, token, sizeof(v.password));
        v.password[sizeof(v.password)-1] = '\0';

        token = strtok(NULL, "|");
        v.voted = token ? atoi(token) : 0;

        if(strcmp(v.regNo, regNo) == 0 && strcmp(v.password, password) == 0) {
            found = 1;
            if(v.voted == 1) {
                votedAlready = 1;
            }
        }
    }

    fclose(vfile);

    if(!found) {
        printf("Voter not registered or wrong credentials.\n");
        return;
    }
    if(votedAlready) {
        printf("You have already voted.\n");
        return;
    }

    // Load all contestants into memory
    FILE *cfile;
    struct Contestant c;
    cfile = fopen("contestants.txt", "r+");
    if(cfile == NULL) {
        printf("No contestants found.\n");
        return;
    }

    char names[100][50]; 
    char regNos[100][20];
    char pos[100][30];
    int votes[100];
    int count = 0;

    while(fgets(line, sizeof(line), cfile) != NULL && count < 100) {
        char *token;

        line[strcspn(line, "\n")] = 0;

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(c.name, token, sizeof(c.name));
        c.name[sizeof(c.name)-1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.regNo, token, sizeof(c.regNo));
        c.regNo[sizeof(c.regNo)-1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(c.position, token, sizeof(c.position));
        c.position[sizeof(c.position)-1] = '\0';

        token = strtok(NULL, "|");
        c.votes = token ? atoi(token) : 0;

        strcpy(names[count], c.name);
        strcpy(regNos[count], c.regNo);
        strcpy(pos[count], c.position);
        votes[count] = c.votes;
        count++;
    }

    if(count == 0) {
        printf("No contestants found.\n");
        fclose(cfile);
        return;
    }

    // Load positions
    FILE *pfile = fopen("positions.txt", "r");
    if(pfile == NULL) {
        printf("No positions found.\n");
        fclose(cfile);
        return;
    }

    char positions[100][30];
    int posCount = 0;

    while(fgets(line, sizeof(line), pfile) != NULL && posCount < 100) {
        line[strcspn(line, "\n")] = 0;
        if(strlen(line) == 0) continue;
        strncpy(positions[posCount], line, sizeof(positions[posCount]));
        positions[posCount][sizeof(positions[posCount]) - 1] = '\0';
        posCount++;
    }

    fclose(pfile);

    if(posCount == 0) {
        printf("No positions defined.\n");
        fclose(cfile);
        return;
    }

    // For each position, let the voter choose a contestant (or skip)
    for(int p = 0; p < posCount; p++) {
        int candidateIdx[100];
        int candidateCount = 0;

        printf("\nPosition: %s\n", positions[p]);

        for(int j = 0; j < count; j++) {
            if(strcmp(pos[j], positions[p]) == 0) {
                candidateIdx[candidateCount] = j;
                candidateCount++;
            }
        }

        if(candidateCount == 0) {
            printf("No contestants available for this position.\n");
            printf("Press Enter to continue to the next position...");
            getchar();
            continue;
        }

        for(int k = 0; k < candidateCount; k++) {
            int idx = candidateIdx[k];
            printf("%d. %s (%s)\n", k + 1, names[idx], regNos[idx]);
        }

        int choice;
        printf("Enter candidate number to vote for (or 0 to skip this position): ");
        scanf("%d", &choice);

        if(choice == 0) {
            // skip this position
        } else if(choice >= 1 && choice <= candidateCount) {
            int chosenIndex = candidateIdx[choice - 1];
            votes[chosenIndex]++;
        } else {
            printf("Invalid choice for this position. Skipping.\n");
        }
    }

    // Rewrite candidates file with updated votes using '|' delimiter
    freopen("contestants.txt", "w", cfile);
    for(int j = 0; j < count; j++) {
        fprintf(cfile, "%s|%s|%s|%d\n", names[j], regNos[j], pos[j], votes[j]);
    }

    fclose(cfile);

    // Phase 2: now that the vote was successfully recorded, mark voter as having voted
    FILE *tempV;
    vfile = fopen("voters.txt", "r");
    tempV = fopen("voters_temp.txt", "w");

    if(vfile == NULL || tempV == NULL) {
        printf("Error updating voter status, but vote was recorded.\n");
        if(vfile) fclose(vfile);
        if(tempV) fclose(tempV);
        return;
    }

    while(fgets(line, sizeof(line), vfile) != NULL) {
        char *token;

        line[strcspn(line, "\n")] = 0;

        token = strtok(line, "|");
        if(!token) continue;
        strncpy(v.name, token, sizeof(v.name));
        v.name[sizeof(v.name)-1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(v.regNo, token, sizeof(v.regNo));
        v.regNo[sizeof(v.regNo)-1] = '\0';

        token = strtok(NULL, "|");
        if(!token) continue;
        strncpy(v.password, token, sizeof(v.password));
        v.password[sizeof(v.password)-1] = '\0';

        token = strtok(NULL, "|");
        v.voted = token ? atoi(token) : 0;

        if(strcmp(v.regNo, regNo) == 0 && strcmp(v.password, password) == 0) {
            v.voted = 1;
        }

        fprintf(tempV, "%s|%s|%s|%d\n", v.name, v.regNo, v.password, v.voted);
    }

    fclose(vfile);
    fclose(tempV);

    remove("voters.txt");
    rename("voters_temp.txt", "voters.txt");

    printf("Vote cast successfully!\n");
}

void tallyVotes() {
    char regNo[20], password[20];

    printf("\n--- Election Results ---\n");

    printf("Enter your Admin Reg No: ");
    getchar();
    fgets(regNo, sizeof(regNo), stdin);
    regNo[strcspn(regNo, "\n")] = 0;

    printf("Enter your Admin Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    // Send to server
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "TALLY_VOTES|%s|%s", regNo, password);

    struct sockaddr_in server_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    socklen_t addr_len = sizeof(server_addr);
    sendto(sock, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)&server_addr, addr_len);

    memset(buffer, 0, sizeof(buffer));
    if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len) > 0) {
        printf("\n%s\n", buffer);
    } else {
        printf("No response from server.\n");
    }
    close(sock);

    printf("\nPress Enter to return to menu...");
    getchar();
}