# OS Simulator with Live Web Dashboard

A comprehensive Operating System simulator written in **C**, featuring a fully dynamic CPU scheduler, memory manager, and concurrency controller. The C backend is bridged to a modern, dark-mode **Web Dashboard** using a **Node.js/Socket.io** server, allowing for real-time visualization of clock cycles, memory states, and system queues.

---

## Key Features

### 1. CPU Scheduling Algorithms
The OS features a fully dynamic scheduler capable of handling varied arrival times and dynamically assigned time slices.
* **Highest Response Ratio Next (HRRN):** Non-preemptive scheduling that mathematically calculates priority based on waiting time and expected service time to prevent starvation.
* **Round Robin (RR):** Preemptive scheduling with a user-defined Time Quantum.
* **Multilevel Feedback Queue (MLFQ):** A strict 4-tier priority queue system (Q0, Q1, Q2, Q3) with cascading time quantums and priority hijacking.

### 2. Memory & Disk Management
* **Main Memory (RAM):** 40-word capacity. Dynamically loads process instructions, variables, and PCB boundaries.
* **Hard Disk Swapping:** 100-word capacity. When RAM reaches maximum capacity, the Memory Manager automatically identifies sleeping/blocked processes, swaps them to the Hard Disk, compacts the RAM, and seamlessly swaps them back when they are scheduled to run.

### 3. Concurrency & Semaphores
Implements strict mutual exclusion using three core binary semaphores:
* `userInput`
* `userOutput`
* `fileAccess`
Processes attempting to access locked resources are immediately preempted, placed into specific Semaphore Blocked Queues, and awoken via `semSignal`.

### 4. Live Web Dashboard
A Node.js server wraps the C executable, streaming the `stdout` buffer directly to a browser-based terminal via WebSockets. It features:
* Dynamic configuration inputs (Arrival Times, Algorithm Choice, Time Quantum).
* Live terminal inputs to feed values back into the running C process.
* Real-time Clock Cycle tracking.

---

## Tech Stack
* **Kernel/Backend:** C 
* **Bridge Server:** Node.js, Express, Socket.io, Child Process API
* **Frontend GUI:** HTML5, CSS3, Vanilla JavaScript

---

## Installation & Setup

### Prerequisites
1. **C Compiler** (e.g., GCC)
2. **Node.js** (v18+ recommended)

### 1. Compile the C Backend
Ensure all `.c` files are compiled into a single executable named `os_sim.exe` (Windows) or `os_sim` (Mac/Linux):
```bash
gcc main.c interpreter.c scheduler.c memory.c queue.c -o os_sim.exe
