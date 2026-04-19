// /* =========================================================
//  * client_tcp.c  —  CONNECTION-ORIENTED client for voting server
//  *
//  * CONNECTION-ORIENTED aspects (look for the ★ markers):
//  *  ★ SOCK_STREAM  – TCP socket; guarantees ordered, reliable delivery
//  *  ★ connect()    – triggers the TCP three-way handshake (SYN /
//  *                   SYN-ACK / ACK). After this returns, a real
//  *                   persistent connection exists between client and server.
//  *  ★ recv()/send() – stream I/O over the established connection
//  *  ★ close()      – sends TCP FIN, initiating graceful teardown
//  *
//  * The client is essentially unchanged from the original — the
//  * connection-oriented work is almost entirely on the socket type
//  * and the connect() call. The stream abstraction hides the rest.
//  * ========================================================= */


// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <netinet/in.h>

// #define PORT        9090
// #define MAX_PAYLOAD 2048

// void ensureAdminExists();
// int verifyAdmin();
// void createAdmin();
// void adminPanel();
// void managePositions();
// void registerVoter();
// void registerContestant();
// void castVote();
// void tallyVotes();
// void displayAdminInfo();

// struct Voter {
//     char name[50];
//     char regNo[20];
//     char password[20];
//     int voted;
// };

// struct Contestant {
//     char name[50];
//     char regNo[20];
//     char position[30];
//     int votes;
// };

// struct Admin {
//     char name[50];
//     char regNo[20];
//     char password[20];
// };

// typedef enum { SVR_DISPLAY = 1, SVR_PROMPT = 2, CLI_INPUT = 3 } MsgType;

// /* Request and Response Types (new client-driven model) */
// typedef enum {
//     REQ_REGISTER_VOTER = 10,
//     REQ_VERIFY_ADMIN = 20,
//     REQ_CAST_VOTE = 30
// } RequestType;

// typedef enum {
//     RESP_SUCCESS = 1,
//     RESP_ERROR = 2,
//     RESP_DATA = 3
// } ResponseType;

// /* Extended message structure to support both old and new protocols */
// typedef struct {
//     int  type;
//     int  req_type;       /* For CLIENT_REQUEST messages */
//     int  resp_status;    /* For SVR_RESPONSE: RESP_SUCCESS/RESP_ERROR/RESP_DATA */
//     char text[MAX_PAYLOAD];
    
//     /* Request payload fields (for client-driven requests) */
//     struct {
//         char name[50];
//         char regNo[20];
//         char password[20];
//     } voter_data;
// } Msg;



// int main(void)
// {
//     /* Prompt the user for the server IP at runtime */
//     char server_ip[64];
//     printf("Enter server IP address: ");
//     fflush(stdout);
//     if (fgets(server_ip, sizeof(server_ip), stdin) == NULL) {
//         fprintf(stderr, "Failed to read IP address.\n");
//         exit(1);
//     }
//     server_ip[strcspn(server_ip, "\r\n")] = '\0';  /* strip newline */

//     /* ★ CONNECTION-ORIENTED: SOCK_STREAM = TCP */
//     int sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock < 0) { perror("socket"); exit(1); }

//     struct sockaddr_in server_addr;
//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port   = htons(PORT);

//     if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
//         fprintf(stderr, "Invalid IP address: %s\n", server_ip);
//         exit(1);
//     }

//     /* ★ connect() — performs the TCP three-way handshake.
//      *   SYN  →  (client to server)
//      *   SYN-ACK ←  (server to client)
//      *   ACK  →  (client to server)
//      *   After this call succeeds, a reliable ordered stream is established.
//      *   The server's accept() unblocks and returns a dedicated socket. */
//     if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//         perror("connect");
//         fprintf(stderr, "Make sure the TCP server is running on %s:%d\n",
//                 server_ip, PORT);
//         exit(1);
//     }

//     printf("[tcp-client] Connected (TCP) to %s:%d\n", server_ip, PORT);

//     /* ============================================================ */
//     /*  CLIENT-SIDE STATE MACHINE: Handle all prompting locally     */
//     /* ============================================================ */
    
//     printf("\n[TCP Voting Client - Client-Driven UI]\n");
//     printf("All prompts are now local to this client.\n");
//     printf("Server only receives requests and sends responses.\n\n");
    
//     int running = 1;
//     while (running) {
//         /* Display main menu locally (NOT from server) */
//         printf("\n=== Main Menu ===\n");
//         printf("1. Admin Panel\n");
//         printf("2. Register Voter\n");
//         printf("3. Exit\n");
//         printf("Enter choice: ");
//         fflush(stdout);
        
//         char choice[20];
//         if (fgets(choice, sizeof(choice), stdin) == NULL) {
//             printf("Error reading input.\n");
//             break;
//         }
//         choice[strcspn(choice, "\n")] = '\0';
//         ensure AdminExists(sock);  /* Ensure admin exists before any actions */

//         if (strcmp(choice, "1") == 0) {
//             /* ---- ADMIN PANEL (client-side input collection) ---- */
//             printf("\n=== Admin Panel ===\n");
//             char regNo[20], password[20];
            
//             printf("Admin Reg No: ");
//             fflush(stdout);
//             if (fgets(regNo, sizeof(regNo), stdin) == NULL) continue;
//             regNo[strcspn(regNo, "\n")] = '\0';
            
//             printf("Admin Password: ");
//             fflush(stdout);
//             if (fgets(password, sizeof(password), stdin) == NULL) continue;
//             password[strcspn(password, "\n")] = '\0';
            
//             /* Validate locally */
//             if (strlen(regNo) == 0 || strlen(password) == 0) {
//                 printf("Error: Empty fields. Please try again.\n");
//                 continue;
//             }
            
//             /* Build and send request */
//             Msg req;
//             memset(&req, 0, sizeof(req));
//             req.type = 99;  /* Custom request type */
//             req.req_type = REQ_VERIFY_ADMIN;
//             strncpy(req.voter_data.regNo, regNo, sizeof(req.voter_data.regNo) - 1);
//             strncpy(req.voter_data.password, password, sizeof(req.voter_data.password) - 1);
            
//             send(sock, &req, sizeof(req), 0);
            
//             /* Receive response */
//             Msg resp;
//             memset(&resp, 0, sizeof(resp));
//             if (recv(sock, &resp, sizeof(resp), 0) > 0) {
//                 printf("\nServer Response: %s\n", resp.text);
//                 if (resp.resp_status == RESP_SUCCESS) {
//                     printf("Admin authentication successful.\n");
//                     printf("(Full admin panel not implemented in this demo.)\n");
//                 }
//             } else {
//                 printf("No response from server.\n");
//                 break;
//             }
            
//         } else if (strcmp(choice, "2") == 0) {
//             /* ---- VOTER REGISTRATION (client-side input collection) ---- */
//             printf("\n=== Voter Registration ===\n");
//             char name[50], regNo[20], password[20];
            
//             printf("Voter name: ");
//             fflush(stdout);
//             if (fgets(name, sizeof(name), stdin) == NULL) continue;
//             name[strcspn(name, "\n")] = '\0';
            
//             printf("Registration number: ");
//             fflush(stdout);
//             if (fgets(regNo, sizeof(regNo), stdin) == NULL) continue;
//             regNo[strcspn(regNo, "\n")] = '\0';
            
//             printf("Password: ");
//             fflush(stdout);
//             if (fgets(password, sizeof(password), stdin) == NULL) continue;
//             password[strcspn(password, "\n")] = '\0';
            
//             /* Validate locally (client-side validation) */
//             if (strlen(name) == 0 || strlen(regNo) == 0 || strlen(password) == 0) {
//                 printf("Error: Empty fields. Please try again.\n");
//                 continue;
//             }
//             if (strlen(password) < 4) {
//                 printf("Error: Password must be at least 4 characters.\n");
//                 continue;
//             }
            
//             /* Build and send request with all data at once */
//             Msg req;
//             memset(&req, 0, sizeof(req));
//             req.type = 99;  /* Custom request type */
//             req.req_type = REQ_REGISTER_VOTER;
//             strncpy(req.voter_data.name, name, sizeof(req.voter_data.name) - 1);
//             strncpy(req.voter_data.regNo, regNo, sizeof(req.voter_data.regNo) - 1);
//             strncpy(req.voter_data.password, password, sizeof(req.voter_data.password) - 1);
            
//             send(sock, &req, sizeof(req), 0);
            
//             /* Receive response */
//             Msg resp;
//             memset(&resp, 0, sizeof(resp));
//             if (recv(sock, &resp, sizeof(resp), 0) > 0) {
//                 printf("\nServer Response: %s\n", resp.text);
//                 if (resp.resp_status == RESP_ERROR) {
//                     printf("(Registration may have failed. Check with server.)\n");
//                 }
//             } else {
//                 printf("No response from server.\n");
//                 break;
//             }
            
//         } else if (strcmp(choice, "3") == 0) {
//             printf("Goodbye!\n");
//             running = 0;
//         } else {
//             printf("Invalid choice. Please try again.\n");
//         }
//     }

//     /* ★ close() — sends TCP FIN, initiates graceful connection teardown */
//     close(sock);
//     return 0;
// }

// void ensureAdminExists() {
//     FILE *file;
//     char line[256];

//     file = fopen("admin.txt", "r");
//     if (file == NULL){
//         printf("\n No admin found. Please create an admin before using the system.\n");
//         createAdmin();
//         return;
//     }

//     if(fgets(line, sizeof(line), file) == NULL) {
//         fclose(file);
//         printf("\n Admin file is empty. Please create an admin.\n");
//         createAdmin();
//         return;
//     }
//     fclose(file);

// }

// void createAdmin() {
//     FILE *file;
//     struct Admin a;

//     file = fopen("admin.txt", "w");
//     if(file == NULL) {
//         printf("\n Error creating admin file.\n");
//         exit(1);
//     }

//     printf("\n--- Create Admin ---\n");

//     printf("Enter Admin Name: ");
//     fgets(a.name, sizeof(a.name), stdin);
//     a.name[strcspn(a.name, "\n")] = 0;

//     printf("Enter Admin Registration Number: ");
//     fgets(a.regNo, sizeof(a.regNo), stdin);
//     a.regNo[strcspn(a.regNo, "\n")] = 0;

//     printf("Enter Admin Password: ");
//     fgets(a.password, sizeof(a.password), stdin);
//     a.password[strcspn(a.password, "\n")] = 0;

//     fprintf(file, "%s|%s|%s\n", a.name, a.regNo, a.password);
//     fclose(file);

//     printf("Admin created successfully.\n");
// }

// int verifyAdmin() {
//     FILE *file;
//     struct Admin a;
//     char line[256];
//     char inputRegNo[20];
//     char inputPassword[20];
//     int authenticated = 0;

//     file = fopen("admin.txt", "r");
//     if(file == NULL) {
//         printf("Admin file not found. Please restart and create an admin.\n");
//         return 0;
//     }

//     printf("\n--- Admin Authentication ---\n");
//     printf("Enter Admin Registration Number: ");
//     fgets(inputRegNo, sizeof(inputRegNo), stdin);
//     inputRegNo[strcspn(inputRegNo, "\n")] = 0;

//     printf("Enter Admin Password: ");
//     fgets(inputPassword, sizeof(inputPassword), stdin);
//     inputPassword[strcspn(inputPassword, "\n")] = 0;

//     while(fgets(line, sizeof(line), file) != NULL) {
//         char *token;

//         line[strcspn(line, "\n")] = 0;

//         token = strtok(line, "|");
//         if(!token) continue;
//         strncpy(a.name, token, sizeof(a.name));
//         a.name[sizeof(a.name)-1] = '\0';

//         token = strtok(NULL, "|");
//         if(!token) continue;
//         strncpy(a.regNo, token, sizeof(a.regNo));
//         a.regNo[sizeof(a.regNo)-1] = '\0';

//         token = strtok(NULL, "|");
//         if(!token) continue;
//         strncpy(a.password, token, sizeof(a.password));
//         a.password[sizeof(a.password)-1] = '\0';

//         if(strcmp(a.regNo, inputRegNo) == 0 && strcmp(a.password, inputPassword) == 0) {
//             authenticated = 1;
//             break;
//         }
//     }

//     fclose(file);

//     if(!authenticated) {
//         printf("Invalid admin credentials.\n");
//         return 0;
//     }

//     return 1;
// }

// void displayAdminInfo() {
//     char regNo[20], password[20];

//     printf("\n--- View Admin Info ---\n");

//     printf("Enter your Admin Reg No: ");
//     getchar();
//     fgets(regNo, sizeof(regNo), stdin);
//     regNo[strcspn(regNo, "\n")] = 0;

//     printf("Enter your Admin Password: ");
//     fgets(password, sizeof(password), stdin);
//     password[strcspn(password, "\n")] = 0;

//     // Send to server
//     char buffer[BUFFER_SIZE];
//     snprintf(buffer, sizeof(buffer), "GET_ADMIN_INFO|%s|%s", regNo, password);

//     struct sockaddr_in server_addr;
//     int sock = socket(AF_INET, SOCK_DGRAM, 0);
//     struct timeval tv;
//     tv.tv_sec = 5;
//     tv.tv_usec = 0;
//     setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

//     socklen_t addr_len = sizeof(server_addr);
//     sendto(sock, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)&server_addr, addr_len);

//     memset(buffer, 0, sizeof(buffer));
//     if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len) > 0) {
//         printf("\n%s\n", buffer);
//     } else {
//         printf("No response from server.\n");
//     }
//     close(sock);

//     printf("Press Enter to return to menu...");
//     getchar();
// }

// void adminPanel() {
//     int choice;

//     printf("\n--- Admin Panel ---\n");

//     // Authenticate once before showing admin actions
//     if(!verifyAdmin()) {
//         printf("Admin authentication failed. Returning to main menu.\n");
//         return;
//     }

//     while(1) {
//         printf("\nAdmin Options:\n");
//         printf("1. Manage Positions\n");
//         printf("2. Register Contestant\n");
//         printf("3. Tally Votes\n");
//         printf("4. View Admin Info\n");
//         printf("5. Back to Main Menu\n");
//         printf("Enter choice: ");
//         scanf("%d", &choice);

//         switch(choice) {
//             case 1:
//             managePositions();
//                 break;
//             case 2:
//             registerContestant();            
//                 break;
//             case 3:
//             tallyVotes();                
//                 break;
//             case 4:
//             displayAdminInfo();
//                 break;
//             case 5:
//                 return;
//             default:
//                 printf("Invalid choice.\n");
//         }
//     }
// }

// void managePositions() {
//     char position[30];

//     printf("\n--- Manage Positions ---\n");
//     printf("Enter position names to add (empty line to finish):\n");

//     int ch;
//     while((ch = getchar()) != '\n' && ch != EOF) { }

//     while(1) {
//         printf("Position: ");
//         if(fgets(position, sizeof(position), stdin) == NULL) {
//             break;
//         }
//         // If just newline, stop
//         if(strcmp(position, "\n") == 0) {
//             break;
//         }
//         position[strcspn(position, "\n")] = 0;
//         if(strlen(position) == 0) {
//             break;
//         }

//         // Send to server
//         char buffer[BUFFER_SIZE];
//         char regNo[20], password[20];

//         printf("Enter your Admin Reg No: ");
//         fgets(regNo, sizeof(regNo), stdin);
//         regNo[strcspn(regNo, "\n")] = 0;

//         printf("Enter your Admin Password: ");
//         fgets(password, sizeof(password), stdin);
//         password[strcspn(password, "\n")] = 0;

//         snprintf(buffer, sizeof(buffer), "ADD_POSITION|%s|%s|%s", regNo, password, position);
        
//         struct sockaddr_in server_addr;
//         int sock = socket(AF_INET, SOCK_DGRAM, 0);
//         struct timeval tv;
//         tv.tv_sec = 5;
//         tv.tv_usec = 0;
//         setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

//         memset(&server_addr, 0, sizeof(server_addr));
//         server_addr.sin_family = AF_INET;
//         server_addr.sin_port = htons(PORT);
//         inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
        
//         socklen_t addr_len = sizeof(server_addr);
//         sendto(sock, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)&server_addr, addr_len);

//         memset(buffer, 0, sizeof(buffer));
//         if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len) > 0) {
//             printf("Server Response: %s\n", buffer);
//         } else {
//             printf("No response from server.\n");
//         }
//         close(sock);
//     }

//     printf("Positions updated.\n");
// }
// void menu() {
//     printf("\nElectronic Voting System\n");
//     printf("1. Admin Panel\n");
//     printf("2. Register Voter\n");
//     printf("3. Cast Vote\n");
//     printf("4. Exit\n");
// }

// void registerVoter() {

//     FILE *file;
//     struct Voter v;
//     char line[256];

//     file = fopen("voters.txt", "a");
//     if(file == NULL) {
//         printf("Error opening file\n");
//         return;
//     }

//     printf("\n--- Voter Registration ---\n");

//     printf("Enter Name: ");
//     getchar();
//     fgets(v.name, sizeof(v.name), stdin);
//     v.name[strcspn(v.name, "\n")] = 0; 

//     printf("Enter Registration Number: ");
//     fgets(v.regNo, sizeof(v.regNo), stdin);
//     v.regNo[strcspn(v.regNo, "\n")] = 0;

//     printf("Enter Password: ");
//     fgets(v.password, sizeof(v.password), stdin);
//     v.password[strcspn(v.password, "\n")] = 0;

//     v.voted = 0;

//     // Use '|' as a delimiter so names can contain spaces
//     fprintf(file, "%s|%s|%s|%d\n", v.name, v.regNo, v.password, v.voted);

//     fclose(file);

//     printf("Voter registered successfully!\n");
//     printf("Press Enter to return to menu...");
//     getchar();
// }

// void registerContestant() {
//     struct Contestant c;
//     char line[256];

//     printf("\n--- Contestant Registration ---\n");

//     printf("Enter Name: ");
//     getchar(); 
//     fgets(c.name, sizeof(c.name), stdin);
//     c.name[strcspn(c.name, "\n")] = 0; 

//     printf("Enter Registration Number: ");
//     fgets(c.regNo, sizeof(c.regNo), stdin);
//     c.regNo[strcspn(c.regNo, "\n")] = 0;

//     printf("Enter Position: ");
//     fgets(c.position, sizeof(c.position), stdin);
//     c.position[strcspn(c.position, "\n")] = 0;

//     char regNo[20], password[20];
//     printf("Enter your Admin Reg No: ");
//     fgets(regNo, sizeof(regNo), stdin);
//     regNo[strcspn(regNo, "\n")] = 0;

//     printf("Enter your Admin Password: ");
//     fgets(password, sizeof(password), stdin);
//     password[strcspn(password, "\n")] = 0;

//     // Send to server
//     char buffer[BUFFER_SIZE];
//     snprintf(buffer, sizeof(buffer), "REGISTER_CONTESTANT|%s|%s|%s|%s|%s", 
//              regNo, password, c.name, c.regNo, c.position);

//     struct sockaddr_in server_addr;
//     int sock = socket(AF_INET, SOCK_DGRAM, 0);
//     struct timeval tv;
//     tv.tv_sec = 5;
//     tv.tv_usec = 0;
//     setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

//     socklen_t addr_len = sizeof(server_addr);
//     sendto(sock, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)&server_addr, addr_len);

//     memset(buffer, 0, sizeof(buffer));
//     if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len) > 0) {
//         printf("\nServer Response: %s\n", buffer);
//     } else {
//         printf("No response from server.\n");
//     }
//     close(sock);

//     printf("Contestant registered!\n");
//     printf("Press Enter to return to menu...");
//     getchar();
// }

// void castVote() {

//     char regNo[20], password[20];
//     int found = 0, votedAlready = 0;

//     printf("\n--- Cast Vote ---\n");

//     printf("Enter your Registration Number: ");
//     getchar();
//     fgets(regNo, sizeof(regNo), stdin);
//     regNo[strcspn(regNo, "\n")] = 0;

//     printf("Enter your Password: ");
//     fgets(password, sizeof(password), stdin);
//     password[strcspn(password, "\n")] = 0;

//     // Phase 1: verify credentials and vote status without modifying file
//     FILE *vfile;
//     struct Voter v;
//     char line[256];
//     vfile = fopen("voters.txt", "r");

//     if(vfile == NULL) {
//         printf("Error opening voter file\n");
//         return;
//     }

//     // Just check credentials and whether this voter has already voted
//     while(fgets(line, sizeof(line), vfile) != NULL) {
//         char *token;

//         line[strcspn(line, "\n")] = 0;

//         token = strtok(line, "|");
//         if(!token) continue;
//         strncpy(v.name, token, sizeof(v.name));
//         v.name[sizeof(v.name)-1] = '\0';

//         token = strtok(NULL, "|");
//         if(!token) continue;
//         strncpy(v.regNo, token, sizeof(v.regNo));
//         v.regNo[sizeof(v.regNo)-1] = '\0';

//         token = strtok(NULL, "|");
//         if(!token) continue;
//         strncpy(v.password, token, sizeof(v.password));
//         v.password[sizeof(v.password)-1] = '\0';

//         token = strtok(NULL, "|");
//         v.voted = token ? atoi(token) : 0;

//         if(strcmp(v.regNo, regNo) == 0 && strcmp(v.password, password) == 0) {
//             found = 1;
//             if(v.voted == 1) {
//                 votedAlready = 1;
//             }
//         }
//     }

//     fclose(vfile);

//     if(!found) {
//         printf("Voter not registered or wrong credentials.\n");
//         return;
//     }
//     if(votedAlready) {
//         printf("You have already voted.\n");
//         return;
//     }

//     // Load all contestants into memory
//     FILE *cfile;
//     struct Contestant c;
//     cfile = fopen("contestants.txt", "r+");
//     if(cfile == NULL) {
//         printf("No contestants found.\n");
//         return;
//     }

//     char names[100][50]; 
//     char regNos[100][20];
//     char pos[100][30];
//     int votes[100];
//     int count = 0;

//     while(fgets(line, sizeof(line), cfile) != NULL && count < 100) {
//         char *token;

//         line[strcspn(line, "\n")] = 0;

//         token = strtok(line, "|");
//         if(!token) continue;
//         strncpy(c.name, token, sizeof(c.name));
//         c.name[sizeof(c.name)-1] = '\0';

//         token = strtok(NULL, "|");
//         if(!token) continue;
//         strncpy(c.regNo, token, sizeof(c.regNo));
//         c.regNo[sizeof(c.regNo)-1] = '\0';

//         token = strtok(NULL, "|");
//         if(!token) continue;
//         strncpy(c.position, token, sizeof(c.position));
//         c.position[sizeof(c.position)-1] = '\0';

//         token = strtok(NULL, "|");
//         c.votes = token ? atoi(token) : 0;

//         strcpy(names[count], c.name);
//         strcpy(regNos[count], c.regNo);
//         strcpy(pos[count], c.position);
//         votes[count] = c.votes;
//         count++;
//     }

//     if(count == 0) {
//         printf("No contestants found.\n");
//         fclose(cfile);
//         return;
//     }

//     // Load positions
//     FILE *pfile = fopen("positions.txt", "r");
//     if(pfile == NULL) {
//         printf("No positions found.\n");
//         fclose(cfile);
//         return;
//     }

//     char positions[100][30];
//     int posCount = 0;

//     while(fgets(line, sizeof(line), pfile) != NULL && posCount < 100) {
//         line[strcspn(line, "\n")] = 0;
//         if(strlen(line) == 0) continue;
//         strncpy(positions[posCount], line, sizeof(positions[posCount]));
//         positions[posCount][sizeof(positions[posCount]) - 1] = '\0';
//         posCount++;
//     }

//     fclose(pfile);

//     if(posCount == 0) {
//         printf("No positions defined.\n");
//         fclose(cfile);
//         return;
//     }

//     // For each position, let the voter choose a contestant (or skip)
//     for(int p = 0; p < posCount; p++) {
//         int candidateIdx[100];
//         int candidateCount = 0;

//         printf("\nPosition: %s\n", positions[p]);

//         for(int j = 0; j < count; j++) {
//             if(strcmp(pos[j], positions[p]) == 0) {
//                 candidateIdx[candidateCount] = j;
//                 candidateCount++;
//             }
//         }

//         if(candidateCount == 0) {
//             printf("No contestants available for this position.\n");
//             printf("Press Enter to continue to the next position...");
//             getchar();
//             continue;
//         }

//         for(int k = 0; k < candidateCount; k++) {
//             int idx = candidateIdx[k];
//             printf("%d. %s (%s)\n", k + 1, names[idx], regNos[idx]);
//         }

//         int choice;
//         printf("Enter candidate number to vote for (or 0 to skip this position): ");
//         scanf("%d", &choice);

//         if(choice == 0) {
//             // skip this position
//         } else if(choice >= 1 && choice <= candidateCount) {
//             int chosenIndex = candidateIdx[choice - 1];
//             votes[chosenIndex]++;
//         } else {
//             printf("Invalid choice for this position. Skipping.\n");
//         }
//     }

//     // Rewrite candidates file with updated votes using '|' delimiter
//     freopen("contestants.txt", "w", cfile);
//     for(int j = 0; j < count; j++) {
//         fprintf(cfile, "%s|%s|%s|%d\n", names[j], regNos[j], pos[j], votes[j]);
//     }

//     fclose(cfile);

//     // Phase 2: now that the vote was successfully recorded, mark voter as having voted
//     FILE *tempV;
//     vfile = fopen("voters.txt", "r");
//     tempV = fopen("voters_temp.txt", "w");

//     if(vfile == NULL || tempV == NULL) {
//         printf("Error updating voter status, but vote was recorded.\n");
//         if(vfile) fclose(vfile);
//         if(tempV) fclose(tempV);
//         return;
//     }

//     while(fgets(line, sizeof(line), vfile) != NULL) {
//         char *token;

//         line[strcspn(line, "\n")] = 0;

//         token = strtok(line, "|");
//         if(!token) continue;
//         strncpy(v.name, token, sizeof(v.name));
//         v.name[sizeof(v.name)-1] = '\0';

//         token = strtok(NULL, "|");
//         if(!token) continue;
//         strncpy(v.regNo, token, sizeof(v.regNo));
//         v.regNo[sizeof(v.regNo)-1] = '\0';

//         token = strtok(NULL, "|");
//         if(!token) continue;
//         strncpy(v.password, token, sizeof(v.password));
//         v.password[sizeof(v.password)-1] = '\0';

//         token = strtok(NULL, "|");
//         v.voted = token ? atoi(token) : 0;

//         if(strcmp(v.regNo, regNo) == 0 && strcmp(v.password, password) == 0) {
//             v.voted = 1;
//         }

//         fprintf(tempV, "%s|%s|%s|%d\n", v.name, v.regNo, v.password, v.voted);
//     }

//     fclose(vfile);
//     fclose(tempV);

//     remove("voters.txt");
//     rename("voters_temp.txt", "voters.txt");

//     printf("Vote cast successfully!\n");
// }

// void tallyVotes() {
//     char regNo[20], password[20];

//     printf("\n--- Election Results ---\n");

//     printf("Enter your Admin Reg No: ");
//     getchar();
//     fgets(regNo, sizeof(regNo), stdin);
//     regNo[strcspn(regNo, "\n")] = 0;

//     printf("Enter your Admin Password: ");
//     fgets(password, sizeof(password), stdin);
//     password[strcspn(password, "\n")] = 0;

//     // Send to server
//     char buffer[BUFFER_SIZE];
//     snprintf(buffer, sizeof(buffer), "TALLY_VOTES|%s|%s", regNo, password);

//     struct sockaddr_in server_addr;
//     int sock = socket(AF_INET, SOCK_DGRAM, 0);
//     struct timeval tv;
//     tv.tv_sec = 5;
//     tv.tv_usec = 0;
//     setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

//     socklen_t addr_len = sizeof(server_addr);
//     sendto(sock, buffer, strlen(buffer) + 1, 0, (struct sockaddr *)&server_addr, addr_len);

//     memset(buffer, 0, sizeof(buffer));
//     if(recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len) > 0) {
//         printf("\n%s\n", buffer);
//     } else {
//         printf("No response from server.\n");
//     }
//     close(sock);

//     printf("\nPress Enter to return to menu...");
//     getchar();
// }

/* =========================================================
 * client_tcp.c  —  CONNECTION-ORIENTED client for voting server
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT        9090
#define MAX_PAYLOAD 2048

/* ── Shared protocol types (must match server) ── */
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

/* ── Forward declarations ── */
void ensureAdminExists(int sock);
void adminPanel(int sock);

/* ================================================================== */
/*  Client I/O helpers — send input to server, receive display msgs   */
/* ================================================================== */

/*
 * Pump the socket: read SVR_DISPLAY / SVR_PROMPT messages and print
 * them until we hit a message that the CALLER needs to handle
 * (resp_status set, or type == CLI_INPUT request from the old path).
 * Returns the last Msg received so the caller can inspect resp_status.
 */
static Msg pump_until_response(int sock)
{
    Msg m;
    while (1) {
        memset(&m, 0, sizeof(m));
        if (recv(sock, &m, sizeof(m), 0) <= 0) {
            printf("[client] Server disconnected.\n");
            m.resp_status = RESP_ERROR;
            return m;
        }

        if (m.type == SVR_DISPLAY) {
            /* Plain server output — just print it */
            printf("%s", m.text);
            fflush(stdout);

        } else if (m.type == SVR_PROMPT) {
            /* Server wants interactive input from us */
            printf("%s", m.text);
            fflush(stdout);
            char input[MAX_PAYLOAD];
            if (fgets(input, sizeof(input), stdin) == NULL)
                input[0] = '\0';
            input[strcspn(input, "\n")] = '\0';

            Msg reply;
            memset(&reply, 0, sizeof(reply));
            reply.type = CLI_INPUT;
            strncpy(reply.text, input, MAX_PAYLOAD - 1);
            send(sock, &reply, sizeof(reply), 0);

        } else {
            /* type == 99 (response) or anything else — return to caller */
            return m;
        }
    }
}

/* Send a simple request (no payload) and pump until response */
static Msg send_request(int sock, RequestType req_type)
{
    Msg req;
    memset(&req, 0, sizeof(req));
    req.type     = 99;
    req.req_type = req_type;
    send(sock, &req, sizeof(req), 0);
    return pump_until_response(sock);
}

/* ================================================================== */
/*  Admin panel (client side — drives server-side admin_panel())      */
/* ================================================================== */
void adminPanel(int sock)
{
    /* Step 1: ask the server to run its admin_panel() for this client.
     * The server will prompt for credentials via SVR_PROMPT, then loop
     * through sub-menu choices also via SVR_PROMPT / SVR_DISPLAY.     */
    printf("\n=== Admin Panel ===\n");

    Msg req;
    memset(&req, 0, sizeof(req));
    req.type     = 99;
    req.req_type = REQ_VERIFY_ADMIN;
    send(sock, &req, sizeof(req), 0);

    /* Pump: server will send prompts for RegNo / Password,
     * then either RESP_SUCCESS or RESP_ERROR.                         */
    Msg auth = pump_until_response(sock);

    if (auth.resp_status == RESP_ERROR) {
        printf("\nAdmin authentication failed. Returning to main menu.\n");
        return;
    }

    /* Step 2: admin is authenticated — now loop the sub-menu locally,
     * sending the appropriate REQ_* for each option and pumping the
     * server's display back.                                          */
    while (1) {
        printf("\nAdmin Options:\n");
        printf("1. Manage Positions\n");
        printf("2. Register Contestant\n");
        printf("3. Tally Votes\n");
        printf("4. View Admin Info\n");
        printf("5. Back to Main Menu\n");
        printf("Enter choice: ");
        fflush(stdout);

        char choice[20];
        if (fgets(choice, sizeof(choice), stdin) == NULL) break;
        choice[strcspn(choice, "\n")] = '\0';

        if (strcmp(choice, "1") == 0) {
            Msg r = send_request(sock, REQ_MANAGE_POSITIONS);
            if (r.text[0]) printf("\n%s\n", r.text);

        } else if (strcmp(choice, "2") == 0) {
            Msg r = send_request(sock, REQ_REG_CONTESTANT);
            if (r.text[0]) printf("\n%s\n", r.text);

        } else if (strcmp(choice, "3") == 0) {
            Msg r = send_request(sock, REQ_TALLY_VOTES);
            if (r.text[0]) printf("\n%s\n", r.text);

        } else if (strcmp(choice, "4") == 0) {
            Msg r = send_request(sock, REQ_VIEW_ADMIN_INFO);
            if (r.text[0]) printf("\n%s\n", r.text);

        } else if (strcmp(choice, "5") == 0) {
            Msg r = send_request(sock, REQ_ADMIN_BACK);
            if (r.text[0]) printf("\n%s\n", r.text);
            break;

        } else {
            printf("Invalid choice.\n");
        }
    }
}

/* ================================================================== */
/*  ensureAdminExists — asks the server to check / create admin       */
/* ================================================================== */
void ensureAdminExists(int sock)
{
    Msg req;
    memset(&req, 0, sizeof(req));
    req.type     = 99;
    req.req_type = REQ_ENSURE_ADMIN;
    send(sock, &req, sizeof(req), 0);

    /* Server will send SVR_DISPLAY / SVR_PROMPT if admin needs creating */
    pump_until_response(sock);
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */
int main(void)
{
    char server_ip[64];
    printf("Enter server IP address: ");
    fflush(stdout);
    if (fgets(server_ip, sizeof(server_ip), stdin) == NULL) {
        fprintf(stderr, "Failed to read IP address.\n");
        exit(1);
    }
    server_ip[strcspn(server_ip, "\r\n")] = '\0';

    /* ★ SOCK_STREAM = TCP */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", server_ip);
        exit(1);
    }

    /* ★ connect() — TCP three-way handshake */
    if (connect(sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        fprintf(stderr, "Make sure the TCP server is running on %s:%d\n",
                server_ip, PORT);
        exit(1);
    }

    printf("[tcp-client] Connected (TCP) to %s:%d\n\n", server_ip, PORT);

    /* Ensure an admin account exists before anything else */
    ensureAdminExists(sock);

    int running = 1;
    while (running) {
        printf("\n=== Main Menu ===\n");
        printf("1. Admin Panel\n");
        printf("2. Register Voter\n");
        printf("3. Exit\n");
        printf("Enter choice: ");
        fflush(stdout);

        char choice[20];
        if (fgets(choice, sizeof(choice), stdin) == NULL) break;
        choice[strcspn(choice, "\n")] = '\0';

        if (strcmp(choice, "1") == 0) {
            /* ── Admin panel: auth + sub-menu handled in adminPanel() ── */
            adminPanel(sock);

        } else if (strcmp(choice, "2") == 0) {
            /* ── Voter registration: collect locally, send to server ── */
            printf("\n=== Voter Registration ===\n");
            char name[50], regNo[20], password[20];

            printf("Voter name: ");       fflush(stdout);
            if (fgets(name, sizeof(name), stdin) == NULL) continue;
            name[strcspn(name, "\n")] = '\0';

            printf("Registration number: "); fflush(stdout);
            if (fgets(regNo, sizeof(regNo), stdin) == NULL) continue;
            regNo[strcspn(regNo, "\n")] = '\0';

            printf("Password: ");         fflush(stdout);
            if (fgets(password, sizeof(password), stdin) == NULL) continue;
            password[strcspn(password, "\n")] = '\0';

            if (!strlen(name) || !strlen(regNo) || !strlen(password)) {
                printf("Error: Empty fields.\n"); continue;
            }
            if (strlen(password) < 4) {
                printf("Error: Password must be at least 4 characters.\n");
                continue;
            }

            Msg req;
            memset(&req, 0, sizeof(req));
            req.type     = 99;
            req.req_type = REQ_REGISTER_VOTER;
            strncpy(req.voter_data.name,     name,     sizeof(req.voter_data.name)     - 1);
            strncpy(req.voter_data.regNo,    regNo,    sizeof(req.voter_data.regNo)    - 1);
            strncpy(req.voter_data.password, password, sizeof(req.voter_data.password) - 1);
            send(sock, &req, sizeof(req), 0);

            Msg resp = pump_until_response(sock);
            printf("\nServer: %s\n", resp.text);

        } else if (strcmp(choice, "3") == 0) {
            printf("Goodbye!\n");
            running = 0;

        } else {
            printf("Invalid choice.\n");
        }
    }

    /* ★ close() — graceful TCP teardown */
    close(sock);
    return 0;
}