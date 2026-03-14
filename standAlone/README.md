# Electronic Voting System

A command-line based electronic voting application built in C that manages elections with admin controls, voter registration, and real-time vote tallying.

---

## Table of Contents

1. [Compilation & Running](#compilation--running)
2. [Program Structure](#program-structure)
3. [Navigation Guide](#navigation-guide)
4. [Data Files](#data-files)
5. [Workflow Examples](#workflow-examples)
6. [Important Notes](#important-notes)

---

## Compilation & Running

### Prerequisites

- GCC compiler (or any C compiler)
- Windows PowerShell, Command Prompt, or Terminal
- Text editor for viewing .txt files

### Step 1: Compile the Program

Navigate to the `standAlone/` directory and compile using:

```bash
gcc -o evoting evoting.c
```

This creates an executable file `evoting.exe` (Windows) or `evoting` (Linux/Mac).

### Step 2: Run the Program

Execute the compiled program:

```bash
.\evoting.exe    # Windows PowerShell
evoting.exe      # Windows Command Prompt
./evoting        # Linux/Mac
```

The program will automatically check if an admin account exists. If not, you'll be prompted to create one.

---

## Program Structure

The voting system has **3 data storage files**:

| File | Purpose | Format |
|------|---------|--------|
| `admin.txt` | Admin credentials | `Name\|RegNo\|Password` |
| `positions.txt` | Election positions (e.g., Governor, Deputy-Governor) | One position per line |
| `voters.txt` | Registered voters & vote status | `Name\|RegNo\|Password\|VotedStatus` |
| `contestants.txt` | Candidates for positions | `Name\|RegNo\|Position\|VoteCount` |

---

## Navigation Guide

### Main Menu

When you start the program, you see:

```
Electronic Voting System
1. Admin Panel
2. Register Voter
3. Cast Vote
4. Exit
```

---

### Option 1: Admin Panel

**Purpose:** Manage election setup and view results (requires authentication)

**Steps:**

1. Select **1** from main menu
2. Enter admin **Registration Number** and **Password**
   - Default admin: Username `Justus Kitavi`, RegNo `SCS3/1/2023`, Password `1234567890`
3. After authentication, you access admin options:

```
Admin Options:
1. Manage Positions
2. Register Contestant
3. Tally Votes
4. Back to Main Menu
```

#### 1a. Manage Positions

- Allows you to define election positions (Governor, Deputy-Governor, etc.)
- Enter each position name and press Enter
- Leave blank and press Enter to finish
- Example positions: `Governor`, `Deputy-Governor`, `Treasurer`

#### 1b. Register Contestant

- Register candidates who will contest for positions
- Input candidate details (Name, Registration Number)
- Select from available positions already created
- Each candidate can only contest for one position

#### 1c. Tally Votes

- Displays all election results
- Shows vote counts for each candidate
- Highlights the winner(s) for each position (most votes)

---

### Option 2: Register Voter

**Purpose:** Register a voter in the system

**Steps:**

1. Select **2** from main menu
2. Enter voter details:
   - Full name
   - Registration number
   - A secure password
3. Voter is now registered and can vote once

**Note:** Registration number and password are used to authenticate during voting.

---

### Option 3: Cast Vote

**Purpose:** Vote for candidates in each position

**Steps:**

1. Select **3** from main menu
2. Enter your **Registration Number** and **Password**
   - System verifies you're registered
   - System checks if you've already voted (prevents duplicate voting)
3. For each position:
   - View available candidates
   - Enter candidate number to vote for (or 0 to skip)
4. Vote is recorded and your voter status is updated to "voted"

**Important:** Once you vote, you **cannot vote again** with the same credentials.

---

### Option 4: Exit

Cleanly exit the program. All data is saved.

---

## Data Files

### admin.txt

Stores admin credentials separated by pipes `|`

```
Justus Kitavi|SCS3/1/2023|1234567890
```

### positions.txt

One election position per line

```
Governor
Deputy-Governor
Treasurer
```

### voters.txt

Voter records with vote status (0 = not voted, 1 = voted)

```
Ryan Mbadi|SCS3/6/2023|1234567890|1
Leo Mbadi|SCS3/7/2023|1234567890|0
```

### contestants.txt

Candidate records with vote counts

```
Jon Doe|SCS3/2/2023|Governor|2
Mary Doe|SCS3/3/2023|Governor|1
Amos Doe|SCS3/4/2023|Deputy-Governor|1
```

---

## Workflow Examples

### Example 1: Setting Up an Election

1. Compile and run: `.\evoting.exe`
2. Authenticate with admin credentials
3. Select **Admin Panel → Manage Positions**
4. Add positions: `President`, `Vice-President`, `Secretary`
5. Select **Admin Panel → Register Contestant**
6. Register candidates for each position

### Example 2: Running a Voting Session

1. Run the program
2. Select **Register Voter** (repeat for multiple voters)
3. Voters select **Cast Vote** with their credentials
4. After voting, admin selects **Tally Votes** to view results

### Example 3: Checking Election Results

1. Authenticate as admin
2. Select **Admin Panel → Tally Votes**
3. View all vote counts and winners displayed

---

## Important Notes

1. **Admin Credentials Are Critical:** The first admin account created is stored in `admin.txt`. Keep credentials safe.

2. **One Vote Per Voter:** Once a voter casts votes, their status is marked as "voted" and they cannot vote again.

3. **File-Based Storage:** All data is stored in `.txt` files in the same directory. Ensure the program has read/write permissions.

4. **Data Persistence:** All changes are permanent. To reset, delete the `.txt` files (they'll be recreated on next run).

5. **Candidate vs. Voter:** 
   - Candidates must be registered by admin for specific positions
   - Voters must register before they can vote
   - They are separate entities (a person can be both)

6. **Positions First:** You must create positions before registering candidates. Candidates need a position to contest for.

7. **Multiple Votes Per Session:** When casting votes, a voter can vote for multiple positions in a single voting session (one candidate per position).

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `Admin file not found` | Create admin credentials when prompted |
| `No positions found` | Admin must add positions before registering candidates |
| `You have already voted` | This voter account has already cast votes |
| `Invalid credentials` | Check registration number and password are correct |
| `Error opening file` | Ensure program has write permissions in the directory |

---

## Team Contacts

For issues or questions, contact the development team or refer to the code comments in `evoting.c`.

