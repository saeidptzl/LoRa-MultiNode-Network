# LoRa Multi-Node Forwarding Network (4-Node Chain)

This repository contains a complete implementation of a **4-node LoRa chain network** with reliable forwarding, SD logging, auto-synchronization, watchdog recovery, and test-based configuration switching.

Nodes:
- **Node 1** – Sender  
- **Node 2** – Forwarder  
- **Node 3** – Forwarder  
- **Node 4** – Final Receiver  

All nodes run on **Arduino MKR WAN 1310** + **SD module on SPI**.

---

## 🌐 Project Description

This system transmits LoRa messages from Node 1 → Node 2 → Node 3 → Node 4 **strictly in order**.  
Each node logs packets into an SD card and extracts **RSSI/SNR** for link-quality analysis.

The network runs 4 test configurations:

| Test | SF | CR |
|------|----|----|
| 1 | 7 | 5 |
| 2 | 7 | 8 |
| 3 | 12 | 5 |
| 4 | 12 | 8 |

### Core Features
- 100 packets per test
- 20-second interval between transmissions
- 10-minute pause between tests
- Automatic synchronization of nodes using:
  - Time-based schedule
  - Packet-based detection
  - Scan-mode fallback
- Watchdog LoRa re-initialization
- SD write retry + safe logging
- SPI conflict mitigation
- Reliable forwarding (forward-first, log-second)

---

## 📁 Repository Structure

(Include the tree structure I gave you)

---

## 🔧 Hardware Setup

- Arduino MKR WAN 1310 (×4)
- Micro SD module
- Wires (short as possible)
- Antenna installed on all nodes
- Optional: 100–220 µF capacitor on SD module 3.3V

See `/hardware/` for diagrams and connection tables.

---

## 🧠 How Synchronization Works

Full explanation → `/docs/timing-and-synchronization.md`

Summary:
- Node 1 drives the test transitions.
- Nodes 2–4 synchronize based on:
  1) Packet content  
  2) Time-based fallback  
  3) Scan-mode on silence  
- Watchdogs re-init LoRa when no packets.

---

## 🐞 Debugging Journey (What went wrong & how we fixed it)

Documented fully → `/docs/known-issues.md`

Short version:
- SD blocking LoRa → fixed by forward-first logging
- Out-of-sync tests → fixed by hybrid sync
- LoRa crash after silence → watchdog recovery
- SD write failures → retry + re-init
- SPI fights → guard delays
- Node resets → scan mode added

---

## 📈 Results
Sample logs included in `/results/example-logs/`.

---

## 🛠 Future Work
See `/docs/future-work.md`

---

## 📜 License
MIT License

