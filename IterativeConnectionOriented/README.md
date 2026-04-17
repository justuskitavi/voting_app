# IterativeConnectionOriented - TCP Voting System

## Overview

This directory implements an **electronic voting system using TCP (Connection-Oriented) sockets** with a **client-driven UI architecture**. The server operates iteratively, handling one client at a time via persistent TCP connections.

### Key Architecture
- **Socket Type**: `SOCK_STREAM` (TCP - connection-oriented, reliable streams)
- **Server Model**: Connection-oriented iterative (one client per accept loop iteration)
- **Communication Model**: Request/Response (client-driven)
- **Network**: Prompts user for server IP at runtime
- **Connection Lifecycle**: TCP three-way handshake → data exchange → graceful close

---

## Files

| File | Purpose |
|------|---------|
| `server.c` | TCP server accepting connections and processing voter/admin requests |
| `client.c` | TCP client with local UI for menu and input collection |
| `makefile` | Build configuration |

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
cd IterativeConnectionOriented
./server
# Output: [tcp-server] Listening on port 9090 (connection-oriented iterative)...
```

### Terminal 2 (Client)
```bash
cd IterativeConnectionOriented
./client
# Prompts: Enter server IP address: [enter 127.0.0.1 or remote IP]
# Then displays local menu
```

---

## How It Works

### Connection Lifecycle

1. **Client initiates TCP connection**:
   ```c
   connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
   // TCP three-way handshake (SYN → SYN-ACK → ACK)
   ```

2. **Server accepts connection**:
   ```c
   int client_fd = accept(server_fd, &client_addr, &client_len);
   handle_client(client_fd);  // Serve this client
   close(client_fd);           // Tear down when done
   ```

3. **Data exchange** on persistent stream (same as UDP logic but reliable/ordered)

4. **Client closes socket**:
   ```c
   close(sock);
   // TCP graceful close (FIN → FIN-ACK → ACK)
   ```

### Client-Driven UI Architecture

**Identical to IterativeConnectionless**, but over reliable TCP streams:

1. **Client displays menu locally** (not from server)
2. **Client collects complete input** before sending
3. **Client builds atomic request** and sends to server
4. **Server receives, processes, responds once** (no prompting)
5. **Client displays response** and loops back

### Key Difference from UDP

| Aspect | IterativeConnectionless (UDP) | IterativeConnectionOriented (TCP) |
|--------|-------------------------------|-----------------------------------|
| **Connection** | Stateless (pseudo-connect) | Stateful (real TCP handshake) |
| **Session Tracking** | IP:port in session table | Per-socket file descriptor |
| **Delivery** | Best-effort (unreliable) | Guaranteed (reliable) |
| **Ordering** | No guarantee | In-order guaranteed |
| **Scalability** | Many simultaneous clients | Sequential (one at a time) |
| **Connection Semantics** | Datagram-based | Stream-based |

### Data Flow Diagram

```
Client                          Server
──────────────────────────────────────────
connect()                       listen()
  ├─ SYN ──────────────────→    
  ←── SYN-ACK ──────────────
  ├─ ACK ──────────────────→    accept()
                                (returns client_fd)
[Display Menu]
[Get user input]
[Validate locally]
[Build request]
   └─ TCP send ──────────→      (on client_fd)
                                handle_client()
                                handle_request()
      ←─ TCP send ────────      net_send_response()
[Display response]
   └─ TCP send ──────────→      [next iteration]
      ←─ TCP send ────────
close()
   └─ FIN ────────────────→      close(client_fd)
      ←─ FIN ─────────────
```

---

## Message Protocol

Identical to UDP version:

### Request Types (Client → Server)
```c
REQ_REGISTER_VOTER = 10   // voter_data: name, regNo, password
REQ_VERIFY_ADMIN = 20     // voter_data: regNo, password
REQ_CAST_VOTE = 30        // TODO: implement vote data fields
```

### Response Types (Server → Client)
```c
RESP_SUCCESS = 1          // Operation succeeded
RESP_ERROR = 2            // Operation failed
RESP_DATA = 3             // Data response
```

### Message Structure
```c
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
```

---

## Server Internals

### Main Loop (Connection-Oriented)
```c
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
bind(server_fd, &addr, sizeof(addr));
listen(server_fd, 1);  // Backlog of 1 pending connection

while (1) {
    int client_fd = accept(server_fd, &client_addr, &client_len);
    // Now we have a dedicated socket for this client
    
    handle_client(client_fd);  // Full interaction happens here
    
    close(client_fd);  // Tear down connection when done
    // Loop back to accept next client
}
```

### Handle Client Function
```c
static void handle_client(int sock)
{
    printf("[tcp-server] Client connected. Waiting for requests...\n");
    
    Msg msg;
    while (1) {
        // Read from the TCP stream
        ssize_t n = recv(sock, &msg, sizeof(msg), 0);
        if (n <= 0) break;  // Client disconnected
        
        // Process the request
        if (msg.req_type > 0) {
            handle_request(sock, &msg);
        }
    }
}
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `handle_client(int sock)` | Loop receiving and processing requests from one client |
| `handle_request(int sock, Msg *req)` | Process request, send response via TCP send() |
| `verify_admin_creds()` | Check admin.txt for credentials |
| `net_send_response()` | Send TCP response (uses send() not sendto()) |

---

## Client Internals

Identical to UDP version:

```c
int sock = socket(AF_INET, SOCK_STREAM, 0);
connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

// Now perform request/response loop
while (running) {
    // Display menu
    // Get input
    // Build request
    send(sock, &req, sizeof(req), 0);  // TCP send
    recv(sock, &resp, sizeof(resp), 0); // TCP recv
    // Display response
}

close(sock);
```

---

## Data Files

Identical to UDP version:

| File | Format | Purpose |
|------|--------|---------|
| `voters.txt` | `name\|regNo\|password\|voted_flag` | Registered voters |
| `admin.txt` | `name\|regNo\|password` | Admin credentials |
| `positions.txt` | One position per line | Voting positions |
| `contestants.txt` | `name\|regNo\|position\|vote_count` | Candidates |

---

## Advantages of This Architecture

✅ **Reliable Delivery** — TCP guarantees all messages arrive  
✅ **Ordered Delivery** — TCP guarantees in-order arrival  
✅ **Persistent Connection** — No need for session table  
✅ **Flow Control** — TCP handles backpressure automatically  
✅ **Error Detection** — TCP detects broken connections immediately  
✅ **Stateless Server** — Server doesn't manage UI state, just processes requests  
✅ **Decoupled** — Client UI changes don't require server changes  

## Limitations

⚠️ **Sequential Processing** — Server handles one client fully before next (no concurrent clients)  
⚠️ **Blocking Calls** — recv() and send() block until operation completes  
⚠️ **Single Backlog** — listen(sock, 1) means only 1 pending connection  
⚠️ **Connection Overhead** — TCP handshake adds latency vs UDP  

---

## TCP vs UDP Comparison

For this voting application:

| Criterion | TCP (This Dir) | UDP (Connectionless) |
|-----------|---|---|
| **Reliability** | ✅ Guaranteed | ⚠️ May lose packets |
| **Scalability** | ⚠️ Sequential | ✅ Many simultaneous |
| **Latency** | ⚠️ Higher (handshake) | ✅ Lower |
| **Complexity** | ✅ Simpler (no session mgmt) | ⚠️ Need session table |
| **Network Environment** | ✅ Any | ✅ Any |
| **Use Case** | ✅ LAN, WAN, internet | ✅ LAN, real-time systems |

**When to use TCP**: Voting over internet, important not to lose votes  
**When to use UDP**: LAN voting where speed > reliability, or broadcast voting  

---

## Testing

### Basic Workflow
1. Start server: `./server`
2. Start client: `./client` → Enter `127.0.0.1`
3. Choose "2. Register Voter"
4. Enter name, regNo, password
5. Observe "Voter registered successfully!"
6. Run `cat voters.txt` to confirm saved
7. Start another client and repeat
8. Note: Server must fully close first client before accepting next (due to iterative design)

### Multi-Client Testing
```bash
Terminal 1: ./server
Terminal 2: ./client     # Client A
Terminal 3: (wait until Client A exits, then) ./client  # Client B
```

---

## Network Considerations

- **Port**: 9090 (hardcoded)
- **Protocol**: TCP (connection-oriented, reliable)
- **Binding**: Server binds to `INADDR_ANY` (all interfaces)
- **Client Connection**: Real TCP three-way handshake to user-specified server IP
- **Firewall**: TCP port 9090 must be open
- **Backlog**: listen() backlog is 1 (only one pending connection queued)

---

## Performance Notes

- **Per-client socket overhead** — Each client uses one dedicated socket file descriptor
- **Sequential processing** — Subsequent clients wait in kernel queue until previous client closes
- **No multi-threading** — Simpler code, but blocking design
- **Message size** — Fixed `Msg` struct (2048 bytes + 3 ints) sent per request/response

---

## Future Enhancements

- [ ] Convert to concurrent model (threading or select())
- [ ] Implement full vote casting flow
- [ ] Add duplicate voter detection
- [ ] Add vote tallying endpoint
- [ ] Increase backlog for pending connections
- [ ] Add TCP keep-alive probes
- [ ] Implement graceful shutdown signal handling
- [ ] Add TLS/SSL encryption

---

## See Also

- [IterativeConnectionless/](../IterativeConnectionless/) — UDP version with concurrent clients
- [serverClientSameMachine/](../serverClientSameMachine/) — Original server-driven UI model
