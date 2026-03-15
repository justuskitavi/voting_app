#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void menu();
void ensureAdminExists();
int verifyAdmin();
void createAdmin();
void adminPanel();
void managePositions();
void registerVoter();
void registerContestant();
void castVote();
void tallyVotes();

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

int main() {
    int choice;

    // Ensure there is at least one admin configured before proceeding
    ensureAdminExists();

    while(1) {
        menu();
        printf("Enter choice: ");
        scanf("%d", &choice);

        switch(choice) {

            case 1:
                adminPanel();
                break;

            case 2:
                registerVoter();
                break;

            case 3:
                castVote();
                break;

            case 4:
                exit(0);

            default:
                printf("Invalid choice\n");
        }
    }
}

void ensureAdminExists() {
    FILE *file;
    char line[256];

    file = fopen("admin.txt", "r");
    if(file == NULL) {
        printf("\nNo admin found. Please create an admin before using the system.\n");
        createAdmin();
        return;
    }

    // Check if file has at least one non-empty line
    if(fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        printf("\nAdmin file is empty. Please create an admin.\n");
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
        printf("Error creating admin file.\n");
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
    getchar(); 
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
        printf("4. Back to Main Menu\n");
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
                return;
            default:
                printf("Invalid choice.\n");
        }
    }
}

void managePositions() {
    FILE *file = fopen("positions.txt", "a");
    char position[30];

    if(file == NULL) {
        printf("Error opening positions file.\n");
        return;
    }

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
        fprintf(file, "%s\n", position);
    }

    fclose(file);
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
    FILE *file;
    struct Contestant c;
    char line[256];
    char positions[100][30];
    int posCount = 0;

    // Open positions file to list available positions
    FILE *pfile = fopen("positions.txt", "r");
    if(pfile == NULL) {
        printf("No positions found. Please create positions first.\n");
        return;
    }

    while(fgets(line, sizeof(line), pfile) != NULL && posCount < 100) {
        line[strcspn(line, "\n")] = 0;
        if(strlen(line) == 0) continue;
        strncpy(positions[posCount], line, sizeof(positions[posCount]));
        positions[posCount][sizeof(positions[posCount]) - 1] = '\0';
        posCount++;
    }

    fclose(pfile);

    if(posCount == 0) {
        printf("No positions defined. Please create positions first.\n");
        return;
    }

    file = fopen("contestants.txt", "a");
    if(file == NULL) {
        printf("Error opening contestants file\n");
        return;
    }

    printf("\n--- Contestant Registration ---\n");

    printf("Enter Name: ");
    getchar(); 
    fgets(c.name, sizeof(c.name), stdin);
    c.name[strcspn(c.name, "\n")] = 0; 

    printf("Enter Registration Number: ");
    fgets(c.regNo, sizeof(c.regNo), stdin);
    c.regNo[strcspn(c.regNo, "\n")] = 0;

    // Show positions list
    printf("\nAvailable Positions:\n");
    for(int i = 0; i < posCount; i++) {
        printf("%d. %s\n", i + 1, positions[i]);
    }

    int posChoice;
    printf("Select position number to contest for: ");
    scanf("%d", &posChoice);

    if(posChoice < 1 || posChoice > posCount) {
        printf("Invalid position choice.\n");
        fclose(file);
        return;
    }

    // Copy chosen position into contestant record
    strncpy(c.position, positions[posChoice - 1], sizeof(c.position));
    c.position[sizeof(c.position) - 1] = '\0';

    c.votes = 0;

    // Use '|' as a delimiter so names/positions can contain spaces
    fprintf(file, "%s|%s|%s|%d\n", c.name, c.regNo, c.position, c.votes);

    fclose(file);

    getchar();

    printf("Contestant registered successfully!\n");
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
    struct Contestant c;
    FILE *file;
    int maxVotes = 0;
    char line[256];

    file = fopen("contestants.txt", "r");
    if(file == NULL) {
        printf("No contestants found.\n");
        return;
    }

    printf("\n--- Election Results ---\n");

    // First, find the maximum votes
    while(fgets(line, sizeof(line), file) != NULL) {
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

        if(c.votes > maxVotes)
            maxVotes = c.votes;
    }

    // Reset file pointer to beginning
    fseek(file, 0, SEEK_SET);

    printf("\nVote Counts:\n");
    while(fgets(line, sizeof(line), file) != NULL) {
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

        printf("%s (%s) for %s - %d votes\n", c.name, c.regNo, c.position, c.votes);
    }

    printf("\nWinner(s):\n");
    fseek(file, 0, SEEK_SET);

    while(fgets(line, sizeof(line), file) != NULL) {
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

        if(c.votes == maxVotes) {
            printf("%s (%s) for %s\n", c.name, c.regNo, c.position);
        }
    }

    fclose(file);

    printf("\nPress Enter to return to menu...");
    getchar();
    getchar();
}