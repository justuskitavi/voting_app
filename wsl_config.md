# WSL Networking Configuration Guide

This guide explains how to set up **Mirrored Networking** for WSL2 (Windows 11 22H2+). This mode allows your WSL server to be visible on your local network (LAN) using your machine's real IP address.

---

## 1. Enable Mirrored Networking
Do this on the **server machine** (the one running the C server in WSL).

### Step 1: Create/Edit `.wslconfig`
Open PowerShell (no admin needed) and run this command to open the configuration file:
```powershell
notepad "$env:USERPROFILE\.wslconfig"
```
Paste the following content, then save and close:
```ini
[wsl2]
networkingMode=mirrored
```

### Step 2: Restart WSL
Apply the changes by shutting down WSL. Wait 5 seconds before reopening your terminal.
```powershell
wsl --shutdown
```
You might need to restart your device if the configuration changes do not take effect immediately.

---

## 2. Firewall Configuration (Admin PowerShell)
Open **PowerShell as Administrator** to run these commands.

### Step 3: Allow Hyper-V Inbound Connections
This command allows inbound traffic to reach the WSL environment through the Hyper-V firewall:
```powershell
New-NetFirewallHyperVRule -DisplayName "WSL LAN Inbound" -Direction Inbound -Action Allow -VMCreatorId ((Get-NetFirewallHyperVVMCreator).VMCreatorId)
```

### Step 4: Allow Application Ports
Open the specific ports used by your applications.

#### **For Group Chat Application (Port 9000)**
```powershell
# Allows TCP traffic for the Group Chat server
New-NetFirewallRule -DisplayName "Group Chat App TCP" -Direction Inbound -Protocol TCP -LocalPort 9000 -Action Allow
New-NetFirewallRule -DisplayName "Group Chat App UDP" -Direction Inbound -Protocol UDP -LocalPort 9000 -Action Allow
```

#### **For Voting Application (Port 9090)**
```powershell
# Allows TCP/UDP traffic for the Voting app
New-NetFirewallRule -DisplayName "Voting App TCP" -Direction Inbound -Protocol TCP -LocalPort 9090 -Action Allow
New-NetFirewallRule -DisplayName "Voting App UDP" -Direction Inbound -Protocol UDP -LocalPort 9090 -Action Allow
```

---

## 3. Identify Server IP
In PowerShell, find your machine's LAN IP address:
```powershell
ipconfig
```
Look for `IPv4 Address` under your **Ethernet** or **Wi-Fi** adapter (e.g., `192.168.1.45`). **This is the IP clients will use to connect.**

To verify mirrored mode is active, run this inside WSL:
```bash
ip addr show eth0
```
You should see your real LAN IP instead of a `172.x.x.x` address.

---

## 4. Running the application
To run the desired application navigate to the directory hosting the application (e.g. cd v4) and follow the instructions in the associated README.md file.