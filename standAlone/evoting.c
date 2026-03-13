#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void menu();
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

int main() {
    int choice;

    while(1) {
        menu();
        printf("Enter choice: ");
        scanf("%d", &choice);

        switch(choice) {

            case 1:
                registerVoter();
                break;

            case 2:
                registerContestant();
                break;

            case 3:
                castVote();
                break;

            case 4:
                tallyVotes();
                break;

            case 5:
                exit(0);

            default:
                printf("Invalid choice\n");
        }
    }
}

void menu() {
    printf("\nElectronic Voting System\n");
    printf("1. Register Voter\n");
    printf("2. Register Contestant\n");
    printf("3. Cast Vote\n");
    printf("4. Tally Votes\n");
    printf("5. Exit\n");
}

void registerVoter() {

    FILE *file;
    struct Voter v;

    file = fopen("voters.txt", "a");
    if(file == NULL) {
        printf("Error opening file\n");
        return;
    }

    printf("\n--- Voter Registration ---\n");

    printf("Enter Name: ");
    getchar(); 
    fgets(v.name, sizeof(v.name), stdin);
    v.name[strcspn(v.name, "\n")] = 0; // Remove trailing newline

    printf("Enter Registration Number: ");
    fgets(v.regNo, sizeof(v.regNo), stdin);
    v.regNo[strcspn(v.regNo, "\n")] = 0;

    printf("Enter Password: ");
    fgets(v.password, sizeof(v.password), stdin);
    v.password[strcspn(v.password, "\n")] = 0;

    v.voted = 0;

    fprintf(file, "%s %s %s %d\n", v.name, v.regNo, v.password, v.voted);

    fclose(file);

    printf("Voter registered successfully!\n");
    printf("Press Enter to return to menu...");
    getchar();
}

void registerContestant() {

    FILE *file;
    struct Contestant c;

    file = fopen("contestants.txt", "a");
    if(file == NULL) {
        printf("Error opening file\n");
        return;
    }

    printf("\n--- Contestant Registration ---\n");

    printf("Enter Name: ");
    getchar(); // Clear leftover newline
    fgets(c.name, sizeof(c.name), stdin);
    c.name[strcspn(c.name, "\n")] = 0; // remove newline

    printf("Enter Registration Number: ");
    fgets(c.regNo, sizeof(c.regNo), stdin);
    c.regNo[strcspn(c.regNo, "\n")] = 0;

    printf("Enter Position: ");
    fgets(c.position, sizeof(c.position), stdin);
    c.position[strcspn(c.position, "\n")] = 0;

    c.votes = 0;

    fprintf(file, "%s %s %s %d\n", c.name, c.regNo, c.position, c.votes);

    fclose(file);

    printf("Contestant registered successfully!\n");
    printf("Press Enter to return to menu...");
    getchar();
}

void castVote() {

    char regNo[20], password[20];
    int found = 0, votedAlready = 0;

    printf("\n--- Cast Vote ---\n");

    printf("Enter your Registration Number: ");
    getchar(); // clear newline
    fgets(regNo, sizeof(regNo), stdin);
    regNo[strcspn(regNo, "\n")] = 0;

    printf("Enter your Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    // Open voter file to check credentials
    FILE *vfile, *tempV;
    struct Voter v;
    vfile = fopen("voters.txt", "r");
    tempV = fopen("voters_temp.txt", "w");

    if(vfile == NULL || tempV == NULL) {
        printf("Error opening voter file\n");
        return;
    }

    while(fscanf(vfile, "%s %s %s %d\n", v.name, v.regNo, v.password, &v.voted) != EOF) {
        if(strcmp(v.regNo, regNo) == 0 && strcmp(v.password, password) == 0) {
            found = 1;
            if(v.voted == 1) {
                votedAlready = 1;
            } else {
                v.voted = 1; // mark voter as voted
            }
        }
        fprintf(tempV, "%s %s %s %d\n", v.name, v.regNo, v.password, v.voted);
    }

    fclose(vfile);
    fclose(tempV);

    // Replace old voter file with updated one
    remove("voters.txt");
    rename("voters_temp.txt", "voters.txt");

    if(!found) {
        printf("Voter not registered or wrong credentials.\n");
        return;
    }
    if(votedAlready) {
        printf("You have already voted.\n");
        return;
    }

    // Display candidates
    FILE *cfile;
    struct Contestant c;
    int i = 1;
    cfile = fopen("contestants.txt", "r+");
    if(cfile == NULL) {
        printf("No contestants found.\n");
        return;
    }

    printf("\nCandidates:\n");
    long positions[100]; // store file positions
    char names[100][50]; // store names
    char regNos[100][20];
    char pos[100][30];
    int votes[100];
    int count = 0;

    while(fscanf(cfile, "%s %s %s %d\n", c.name, c.regNo, c.position, &c.votes) != EOF) {
        printf("%d. %s - %s\n", i, c.name, c.position);
        strcpy(names[i-1], c.name);
        strcpy(regNos[i-1], c.regNo);
        strcpy(pos[i-1], c.position);
        votes[i-1] = c.votes;
        i++;
        count++;
    }

    int choice;
    printf("Enter candidate number to vote for: ");
    scanf("%d", &choice);

    if(choice < 1 || choice > count) {
        printf("Invalid choice.\n");
        fclose(cfile);
        return;
    }

    // Increment vote count
    votes[choice-1]++;

    // Rewrite candidates file with updated votes
    freopen("contestants.txt", "w", cfile);
    for(int j=0; j<count; j++) {
        fprintf(cfile, "%s %s %s %d\n", names[j], regNos[j], pos[j], votes[j]);
    }

    fclose(cfile);

    printf("Vote cast successfully!\n");
}

void tallyVotes() {

    struct Contestant c;
    FILE *file;
    int maxVotes = 0;

    file = fopen("contestants.txt", "r");
    if(file == NULL) {
        printf("No contestants found.\n");
        return;
    }

    printf("\n--- Election Results ---\n");

    // First, find the maximum votes
    while(fscanf(file, "%s %s %s %d\n", c.name, c.regNo, c.position, &c.votes) != EOF) {
        if(c.votes > maxVotes)
            maxVotes = c.votes;
    }

    // Reset file pointer to beginning
    fseek(file, 0, SEEK_SET);

    printf("\nVote Counts:\n");
    while(fscanf(file, "%s %s %s %d\n", c.name, c.regNo, c.position, &c.votes) != EOF) {
        printf("%s (%s) for %s - %d votes\n", c.name, c.regNo, c.position, c.votes);
    }

    printf("\nWinner(s):\n");
    fseek(file, 0, SEEK_SET);

    while(fscanf(file, "%s %s %s %d\n", c.name, c.regNo, c.position, &c.votes) != EOF) {
        if(c.votes == maxVotes) {
            printf("%s (%s) for %s\n", c.name, c.regNo, c.position);
        }
    }

    fclose(file);

    printf("\nPress Enter to return to menu...");
    getchar();
    getchar();
}