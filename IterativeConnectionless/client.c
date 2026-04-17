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

        if (strcmp(choice, "1") == 0) {
            /* ---------------- ADMIN ---------------- */
            char regNo[20], password[20];

            printf("\nAdmin Reg No: ");
            fgets(regNo, sizeof(regNo), stdin);
            regNo[strcspn(regNo, "\n")] = '\0';

            printf("Admin Password: ");
            fgets(password, sizeof(password), stdin);
            password[strcspn(password, "\n")] = '\0';

            if (strlen(regNo) == 0 || strlen(password) == 0) {
                printf("Error: Empty fields\n");
                continue;
            }

            /* ✅ Build message safely */
            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer),
                     "VERIFY_ADMIN|%s|%s",
                     regNo, password);

            /* ✅ sendto() */
            sendto(sock, buffer, strlen(buffer) + 1, 0,
                   (struct sockaddr *)&server_addr, addr_len);

            /* ✅ receive */
            memset(buffer, 0, sizeof(buffer));
            if (recvfrom(sock, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&server_addr, &addr_len) > 0) {
                printf("\nServer Response: %s\n", buffer);
            } else {
                printf("No response from server.\n");
            }

        } else if (strcmp(choice, "2") == 0) {
            /* ---------------- REGISTER ---------------- */
            char name[50], regNo[20], password[20];

            printf("\nVoter Name: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0';

            printf("Registration Number: ");
            fgets(regNo, sizeof(regNo), stdin);
            regNo[strcspn(regNo, "\n")] = '\0';

            printf("Password: ");
            fgets(password, sizeof(password), stdin);
            password[strcspn(password, "\n")] = '\0';

            if (strlen(name) == 0 || strlen(regNo) == 0 || strlen(password) == 0) {
                printf("Error: Empty fields\n");
                continue;
            }

            if (strlen(password) < 4) {
                printf("Password too short\n");
                continue;
            }

            /* ✅ Build message */
            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer),
                     "REGISTER|%s|%s|%s",
                     name, regNo, password);

            sendto(sock, buffer, strlen(buffer) + 1, 0,
                   (struct sockaddr *)&server_addr, addr_len);

            /* ✅ receive */
            memset(buffer, 0, sizeof(buffer));
            if (recvfrom(sock, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&server_addr, &addr_len) > 0) {
                printf("\nServer Response: %s\n", buffer);
            } else {
                printf("No response from server.\n");
            }

        } else if (strcmp(choice, "3") == 0) {
            printf("Goodbye!\n");
            running = 0;
        } else {
            printf("Invalid choice.\n");
        }
    }

    close(sock);
    return 0;
}