# serverClientSameMachine - TCP Voting System (Server-Driven UI)

## Overview

This directory implements an **electronic voting system using TCP sockets** designed to run **on a single machine** with a **traditional server-driven UI model**. Unlike the refactored versions (IterativeConnectionless and IterativeConnectionOriented), **all prompts and menu logic originate from the server**, making the client a thin "dumb terminal."

### Key Architecture
- **Socket Type**: `SOCK_STREAM` (TCP - connection-oriented, reliable)
- **Server Model**: Connection-oriented iterative
- **Communication Model**: Server-Driven (traditional client-server)
- **Network**: Hardcoded to `127.0.0.1` (localhost only)
- **Use Case**: Single-machine demo voting booth
- **UI Location**: Server-side (all menus and prompts)

---

## Files

| File | Purpose |
|------|---------|
| `server.c` | TCP server handling all voting logic, menus, and user interaction |
| `client.c` | TCP client acting as terminal for server input/output |
| `makefile` | Build configuration |
| `admin.txt` | Pre-configured admin credentials |

## Compilation

```bash
make server              # Compile TCP server only
make client              # Compile TCP client only
make whole               # Clean, compile both, clear terminal
make clean               # Remove binaries
```

---

## Running the System

### Terminal 1 (Server)
```bash
cd serverClientSameMachine
./server
# Output: [tcp-server] Listening on port 9090 (connection-oriented iterative)...
# Waits for client connection
```

### Terminal 2 (Client)
```bash
cd serverClientSameMachine
./client
# Automatically connects to 127.0.0.1:9090
# Displays server menu
```

---

## How It Works

### Server-Driven Communication Model

The **server completely drives the interaction** via a request-response loop:

```
Server                          Client
──────────────────────────────────────────
accept(client_fd)
[Main menu logic]
  │
  ├─ net_print() ────────→     [Display on terminal]
  │  "1. Admin Panel\n"
  │  "2. Register Voter\n"
  │  "Enter choice: "
  │
  ├─ net_gets() ──────────→    [Prompt for input]
  │  (waits for response)
  │                             [User types]
  │                             [send CLI_INPUT]
  ├─ recv() ◄──────────────    [Input received]
  │  (gets CLI_INPUT)
  │
  ├─ switch (choice) {
  │    case 1: admin_panel()
  │    case 2: register_voter()
  │    case 3: exit
  │
[Loop or close]
```

### Data Flow: Traditional Model

1. **Server sends menu** via `SVR_DISPLAY` and `SVR_PROMPT` messages
2. **Client receives and displays** text on terminal
3. **Client reads stdin** and sends `CLI_INPUT` message with user's choice
4. **Server processes choice**, performs business logic
5. **Server sends next prompt** or result
6. **Repeat until exit**

Unlike the refactored models, the client does **NOT** collect all input locally before sending. Instead, it waits for each server prompt and responds immediately.

### Example Workflow: Voter Registration

```
Server                          Client
──────────────────────────────────────────
[admin_panel logic]
net_print("Voter name: ")  ───→ Displays "Voter name: "
net_gets() waits            ←─── User types "Alice" + Enter
[Got "Alice"]
net_print("Reg No: ")      ───→ Displays "Reg No: "
net_gets() waits            ←─── User types "123" + Enter
[Got "123"]
net_print("Password: ")    ───→ Displays "Password: "
net_gets() waits            ←─── User types "secret" + Enter
[Got "secret"]
fprintf(voters.txt)
net_print("Registered!")   ───→ Displays "Registered!"
```

---

## Message Protocol

### Message Types

```c
typedef enum { 
    SVR_DISPLAY = 1,   // Server sends display text (no input expected)
    SVR_PROMPT = 2,    // Server sends prompt and waits for input
    CLI_INPUT = 3      // Client sends user input response
} MsgType;
```

### Message Structure

```c
typedef struct {
    int  type;              // SVR_DISPLAY, SVR_PROMPT, or CLI_INPUT
    char text[MAX_PAYLOAD]; // Actual text (prompt, display, or user input)
} Msg;
```

### Interaction Pattern

1. Server sends `SVR_DISPLAY` with menu text:
   ```
   { type: SVR_DISPLAY, text: "\n=== Main Menu ===\n1. Admin\n2. Register\n3. Exit\n" }
   ```

2. Server sends `SVR_PROMPT` with prompt text:
   ```
   { type: SVR_PROMPT, text: "Enter choice: " }
   ```

3. Client sends `CLI_INPUT` with user response:
   ```
   { type: CLI_INPUT, text: "2\n" }
   ```

4. Server receives input, makes decision, repeats

---

## Server Internals

### Main Loop
```c
while (1) {
    int client_fd = accept(server_fd, &client_addr, &client_len);
    printf("[tcp-server] Client connected\n");
    handle_client(client_fd);
    close(client_fd);
}
```

### Handle Client Function
```c
static void handle_client(int sock)
{
    ensure_admin_exists(sock);  // Create admin if missing
    
    while (1) {
        net_print(sock, "\n=== Main Menu ===\n1. Admin Panel\n...\n");
        int choice = net_getint(sock, "Enter choice: ");
        
        switch (choice) {
            case 1: admin_panel(sock);     break;
            case 2: register_voter(sock);  break;
            case 3: cast_vote(sock);       break;
            case 4: net_print(sock, "Goodbye!\n"); return;
            default: net_print(sock, "Invalid choice.\n");
        }
    }
}
```

### Major Functions

| Function | Purpose |
|----------|---------|
| `ensure_admin_exists()` | Check if admin.txt exists; create admin if not |
| `admin_panel()` | Display admin menu and authentication |
| `verify_admin()` | Prompt for and verify admin credentials |
| `manage_positions()` | Add positions to positions.txt |
| `register_contestant()` | Register candidate for position |
| `cast_vote()` | Full voting flow: verify voter, display candidates, record votes |
| `tally_votes()` | Display election results |
| `register_voter()` | Prompt and save new voter |

### Network Helper Functions

| Function | Purpose |
|----------|---------|
| `net_print(sock, fmt, ...)` | Send `SVR_DISPLAY` message with formatted text |
| `net_gets(sock, prompt, buf, size)` | Send `SVR_PROMPT`, wait for `CLI_INPUT` response |
| `net_getint(sock, prompt)` | Prompt for integer input |
| `net_pause(sock, msg)` | Wait for user to press Enter before continuing |

---

## Client Internals

### Main Loop
```c
int sock = socket(AF_INET, SOCK_STREAM, 0);
connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
// Hardcoded: server_addr.sin_addr.s_addr = inet_addr("127.0.0.1")

Msg msg;
while (1) {
    recv(sock, &msg, sizeof(msg), 0);  // Wait for server message
    
    switch (msg.type) {
        case SVR_DISPLAY:
            printf("%s", msg.text);  // Just display
            fflush(stdout);
            break;
            
        case SVR_PROMPT:
            printf("%s", msg.text);  // Display prompt
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin) == NULL) {
                input[0] = '\n'; input[1] = '\0';
            }
            // Send user input back to server
            Msg reply = { CLI_INPUT, input };
            send(sock, &reply, sizeof(reply), 0);
            break;
    }
}
close(sock);
```

### Client Responsibilities (Minimal)
1. Connect to hardcoded localhost:9090
2. Display server-sent text on terminal
3. Read stdin when prompted
4. Send input back to server
5. Repeat until disconnected

---

## Data Files

| File | Format | Purpose |
|------|--------|---------|
| `voters.txt` | `name\|regNo\|password\|voted_flag` | Registered voters |
| `admin.txt` | `name\|regNo\|password` | Admin credentials |
| `positions.txt` | One position per line | Voting positions |
| `contestants.txt` | `name\|regNo\|position\|vote_count` | Candidates |

### Initial admin.txt
```
Admin|admin123|admin123
```

---

## Complete Voting Workflow

### Scenario: First-Time Voting

1. **Start server/client**
   - Server runs `handle_client()`
   - Client connects and enters message loop

2. **User chooses "2. Register Voter"**
   - Server: "Voter name: " → Client displays, waits for input
   - User: types "Alice"
   - Server: "Reg No: " → Client displays, waits
   - User: types "001"
   - Server: "Password: " → Client displays, waits
   - User: types "pass123"
   - Server: Saves to voters.txt, displays "Registered!"

3. **User chooses "3. Cast Vote"**
   - Server: Prompts for regNo and password
   - User: enters Alice's credentials
   - Server: Loads positions and candidates from files
   - Server: For each position, displays candidates and prompts for vote
   - User: votes for each position
   - Server: Updates vote counts in contestants.txt
   - Server: Marks voter as voted in voters.txt

4. **User chooses "1. Admin Panel"**
   - Server: "Admin Reg No: " → waits for input
   - User: types "admin123"
   - Server: "Admin Password: " → waits for input
   - User: types "admin123"
   - Server: If valid, displays admin menu
   - Server: Options: manage positions, register contestant, tally votes, back
   - User: Selects option
   - Server: Performs action, displays results

---

## Advantages of This Model

✅ **Simple Code** — All logic in one place on server  
✅ **Easy to Debug** — Server controls entire flow  
✅ **Thin Client** — Client code minimal, mostly I/O  
✅ **Centralized Control** — Server enforces all business rules  
✅ **Localhost Only** — No network complexity  

## Disadvantages

⚠️ **Tight Coupling** — Server and client are interdependent  
⚠️ **UI/Logic Mixed** — Hard to change UI without server changes  
⚠️ **Sequential Clients** — Must close one client before next connects  
⚠️ **No Scalability** — Only works on single machine  
⚠️ **High Latency** — Prompt-response dance for each input field  
⚠️ **Complex Server** — Many functions, state tracking per client  

---

## Comparison: Three Models

| Feature | Connectionless (UDP) | ConnectionOriented (TCP) | SameMachine (TCP) |
|---------|---|---|---|
| **UI Location** | Client-driven | Client-driven | Server-driven |
| **Transport** | UDP | TCP | TCP |
| **Scalability** | Many simultaneous | Sequential | Sequential |
| **Network** | Full network | Full network | Localhost only |
| **Coupling** | Decoupled | Decoupled | Tightly coupled |
| **Code Complexity** | Moderate | Moderate | High |
| **Use Case** | Distributed voting | Reliable voting | Demo/single-machine |

---

## Testing

### Basic Workflow
1. Terminal 1: `./server`
2. Terminal 2: `./client`
3. Choose "1. Admin Panel" → admin123 / admin123 (from admin.txt)
4. Choose "2. Manage Positions" → Enter positions (e.g., "President", "Secretary")
5. Choose "4. Back"
6. Choose "2. Register Voter" → Enter voter details
7. Choose "3. Cast Vote" → Log in as voter, vote for each position
8. Choose "1. Admin Panel" → "3. Tally Votes" → See results

### Verify Data
```bash
cat voters.txt       # See registered voters with voted flag
cat admin.txt        # See admin account
cat positions.txt    # See positions
cat contestants.txt  # See candidates and vote counts
```

---

## Network Considerations

- **Port**: 9090 (hardcoded)
- **Protocol**: TCP (connection-oriented)
- **Server Binding**: `INADDR_ANY` (listens on all interfaces)
- **Client Connection**: Hardcoded to `127.0.0.1` (localhost only)
- **Firewall**: Not needed (localhost communication)

---

## Architectural Evolution

This directory represents the **original server-driven model**. The other two directories show a refactored architecture:

```
Original (serverClientSameMachine)
    ↓
    Server-driven UI
    Client is thin terminal
    Tight coupling
    ↓
Refactored (ConnectionlessIterative, ConnectionOrientedIterative)
    ↓
Client-driven UI
    Server is stateless processor
    Decoupled architecture
    Can run on network
```

---

## Future Enhancements

- [ ] Convert to concurrent model (threading)
- [ ] Extract UI logic from server into separate module
- [ ] Add network support (change hardcoded IP to parameter)
- [ ] Implement authentication/encryption
- [ ] Add multi-round voting support
- [ ] Implement vote verification/auditing
- [ ] Add performance metrics logging

---

## See Also

- [IterativeConnectionless/](../IterativeConnectionless/) — UDP version with client-driven UI
- [IterativeConnectionOriented/](../IterativeConnectionOriented/) — TCP version with client-driven UI
- [Architectural Analysis](../wsl_config.md) — Overview of all models
