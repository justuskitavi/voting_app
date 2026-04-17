# IterativeConnectionless - UDP Voting System

## Overview

This directory implements an **electronic voting system using UDP (Connectionless) sockets** with a **client-driven UI architecture**. The server operates iteratively, handling multiple clients via a session table without establishing persistent connections.

### Key Architecture
- **Socket Type**: `SOCK_DGRAM` (UDP - connectionless, unreliable datagrams)
- **Server Model**: Connectionless iterative (single loop, all clients share socket)
- **Communication Model**: Request/Response (client-driven)
- **Network**: Prompts user for server IP at runtime

---

## Files

| File | Purpose |
|------|---------|
| `server.c` | UDP server handling voter registration and admin verification |
| `client.c` | UDP client with local UI for menu and input collection |
| `makefile` | Build configuration |

## Compilation

```bash
make server              # Compile UDP server only
make client              # Compile UDP client only
make whole               # Clean, compile both, clear terminal
make clean               # Remove binaries
```

---

## Running the System

### Terminal 1 (Server)
```bash
cd IterativeConnectionless
./server
# Output: [udp-server] Listening on port 9090 (connectionless iterative)...
```

### Terminal 2 (Client)
```bash
cd IterativeConnectionless
./client
# Prompts: Enter server IP address: [enter 127.0.0.1 or remote IP]
# Then displays local menu
```

---

## How It Works

### Client-Driven UI Architecture

Unlike traditional server-driven models, **all prompts and menus originate from the client**. The workflow:

1. **Client displays menu locally** (not from server):
   ```
   === Main Menu ===
   1. Admin Panel
   2. Register Voter
   3. Exit
   Enter choice:
   ```

2. **Client collects complete input** before sending:
   - For voter registration: name, regNo, password (all entered locally)
   - For admin: regNo, password (all entered locally)
   - Input validation happens on client (non-empty fields, password length ≥ 4)

3. **Client builds atomic request** and sends to server:
   ```c
   Msg req = {
       .req_type = REQ_REGISTER_VOTER,
       .voter_data = { "John", "123", "pass" }
   };
   send(sock, &req, sizeof(req), 0);
   ```

4. **Server receives request, processes, responds once** (no prompting):
   ```c
   // Server side: pure data processing, no UI
   handle_request(sock, cs, &msg);  
   // → net_send_response(sock, addr, RESP_SUCCESS, "Registered!")
   ```

5. **Client displays server response** and loops back to menu

### Data Flow Diagram

```
Client                          Server
──────────────────────────────────────────
[Display Menu]
[Get user input]
[Validate locally]
[Build request] ──────────→ [Receive request]
                             [Validate data]
                             [Process (file I/O)]
          ←──────── [Send response]
[Display response]
[Loop to menu]
```

### Session Management (UDP Specific)

Since UDP has no real connection, the server maintains a **ClientSession table** to track each client by IP:port:

```c
typedef struct {
    struct sockaddr_in addr;      // Client IP:port
    int  active;                  // Is this session active?
    int  step;                    // Legacy state tracking (now minimal)
    char regNo[20];               
    char password[20];
    char name[50];
} ClientSession;
```

- **Up to 64 concurrent clients** can connect simultaneously
- Session is created on first datagram from new IP:port
- Session persists until client timeout or explicit exit

---

## Message Protocol

### Request Types (Client → Server)
```c
REQ_REGISTER_VOTER = 10   // voter_data: name, regNo, password
REQ_VERIFY_ADMIN = 20     // voter_data: regNo, password
REQ_CAST_VOTE = 30        // TODO: implement vote data fields
```

### Response Types (Server → Client)
```c
RESP_SUCCESS = 1          // Operation succeeded, message provided
RESP_ERROR = 2            // Operation failed, error message provided
RESP_DATA = 3             // Data response with result
```

### Message Structure
```c
typedef struct {
    int  type;                    // Message envelope type
    int  req_type;               // Request type (client sets, server reads)
    int  resp_status;            // Response status (server sets, client reads)
    char text[MAX_PAYLOAD];      // Response message or data
    struct {
        char name[50];           // Voter name
        char regNo[20];          // Registration number
        char password[20];       // Password
    } voter_data;
} Msg;
```

---

## Server Internals

### Main Loop
```c
while (1) {
    recvfrom(sock, &msg, sizeof(msg), 0, 
             (struct sockaddr *)&client_addr, &client_len);
    
    ClientSession *cs = find_or_create_session(&client_addr);
    
    if (msg.req_type > 0) {
        handle_request(sock, cs, &msg);  // Process request, send response
    }
}
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `find_or_create_session()` | Lookup or create session entry for IP:port |
| `handle_request()` | Dispatch request to appropriate handler |
| `verify_admin()` | Check admin credentials against admin.txt |
| `save_voter()` | Append voter to voters.txt |
| `net_send_response()` | Send UDP response datagram to client |

---

## Client Internals

### Main Loop
```c
int running = 1;
while (running) {
    printf("\n=== Main Menu ===\n");
    printf("1. Admin Panel\n");
    printf("2. Register Voter\n");
    printf("3. Exit\n");
    printf("Enter choice: ");
    
    // Read choice
    // Collect data based on choice
    // Validate locally
    // Build request Msg
    // Send request
    // Receive response
    // Display response
}
```

---

## Data Files

| File | Format | Purpose |
|------|--------|---------|
| `voters.txt` | `name\|regNo\|password\|voted_flag` | Registered voters |
| `admin.txt` | `name\|regNo\|password` | Admin credentials |
| `positions.txt` | One position per line | Voting positions |
| `contestants.txt` | `name\|regNo\|position\|vote_count` | Candidates |

### Example voters.txt
```
Alice|001|pass123|0
Bob|002|pass456|1
```

---

## Advantages of This Architecture

✅ **Stateless Server** — Server doesn't manage UI state, just processes requests  
✅ **Scalable** — Can handle many clients simultaneously (limited by session table)  
✅ **Decoupled** — Client UI changes don't require server changes  
✅ **Network Transparent** — Works across networks (UDP routing permitting)  
✅ **Client Validation** — Input errors caught before network round-trip  

## Limitations

⚠️ **No Delivery Guarantee** — UDP datagrams may be lost  
⚠️ **No Order Guarantee** — Datagrams may arrive out of order  
⚠️ **No Flow Control** — Fast client can overwhelm server  
⚠️ **Session Timeout** — No TCP-style keep-alive; stale sessions linger  
⚠️ **Session Limit** — Max 64 concurrent sessions  

---

## Testing

### Basic Workflow
1. Start server: `./server`
2. Start client: `./client` → Enter `127.0.0.1`
3. Choose "2. Register Voter"
4. Enter name, regNo, password when prompted
5. Observe server response: "Voter registered successfully!"
6. Choose "1. Admin Panel"
7. Observe server response for admin credentials

### Verification
- Run `cat voters.txt` to confirm voter was saved
- Run `cat admin.txt` to see admin credentials
- Multiple clients can connect simultaneously (test with multiple terminals)

---

## Network Considerations

- **Port**: 9090 (hardcoded)
- **Protocol**: UDP (connectionless, best-effort)
- **Binding**: Server binds to `INADDR_ANY` (all interfaces)
- **Client Connection**: Pseudo-connect to user-specified server IP
- **Firewall**: UDP port 9090 must be open

---

## Future Enhancements

- [ ] Implement full vote casting flow (`REQ_CAST_VOTE` handler)
- [ ] Add duplicate voter detection
- [ ] Add session timeout mechanism
- [ ] Implement vote tallying endpoint
- [ ] Add encryption/authentication beyond plaintext passwords
- [ ] Increase session table size dynamically
- [ ] Add UDP retransmission for critical messages

---

## See Also

- [IterativeConnectionOriented/](../IterativeConnectionOriented/) — TCP version with persistent connections
- [serverClientSameMachine/](../serverClientSameMachine/) — Original server-driven UI model
