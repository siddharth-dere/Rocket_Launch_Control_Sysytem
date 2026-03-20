# 🚀 RocketSim — Linux IPC-Based Rocket Launch Simulation

A simulation of a rocket launch control system built entirely in **C** using **Linux System Calls and POSIX APIs**. No hardware required — all sensor data is simulated using random values, timers, and shared memory.

---

## Overview

RocketSim simulates a real rocket launch sequence where a **parent process** acts as the Launch Director and a **child process** manages sensor filling (fuel, pressure, temperature). Commands are sent from a second terminal in real time using a Named FIFO pipe.

The system demonstrates several core Operating System concepts including:

- Process creation and management using `fork()` and `waitpid()`
- Multithreading with POSIX `pthreads`
- Inter-Process Communication using 5 different IPC mechanisms
- Signal handling using `sigaction`
- File logging using `open`, `write`, `close`
- Modular software design

---

## Architecture Diagram

```
  Terminal 2 (control tool)
  ./control start
  ./control fire          Named FIFO (/tmp/rkt_cmd)
  ./control abort   ──────────────────────────────►  main.c
  ./control status                                   (parent process)
                                                        │
                                              Unnamed Pipe
                                              (sends shm_id + msg_id)
                                                        │
                                                        ▼
                                                   sensor.c
                                                  (child process)
                                          ┌─────────────┴──────────────┐
                                    fuel_thread   pressure_thread   temp_thread
                                          └─────────────┬──────────────┘
                                                        │
                                          Message Queue (FILL_DONE)
                                                        │
                                                        ▼
                                             Checking Phase (5s)
                                                        │
                                                        ▼
                                             Countdown 10 → 1
                                                        │
                                                        ▼
                                               Flight Loop
                                          (speed + altitude / sec)
                                                        │
                                          Shared Memory ◄──► Semaphore
                                 (fuel, pressure, temp, speed, altitude)
```

---

## System Workflow

### 1. Launch Director — `main.c` (Parent Process)

The Launch Director is the parent process that manages the entire simulation.

**Responsibilities:**
- Creates all IPC resources — shared memory, semaphore, message queue, FIFO, log file
- Forks the sensor child process using `fork()`
- Sends `shm_id` and `msg_id` to the child via an unnamed pipe
- Listens for operator commands via FIFO in a dedicated thread
- Drives the state machine: IDLE → FILLING → CHECKING → READY → COUNTDOWN → FLYING
- Waits for `FILL_DONE` message from child via message queue
- Runs the 5-second checking phase and 10-second countdown
- Updates speed and altitude every second during flight
- Reaps the child process using `waitpid()` — zero zombie processes
- Cleans up all IPC resources on shutdown

---

### 2. Sensor Process — `sensor.c` (Child Process)

The Sensor process simulates three rocket systems filling simultaneously.

**Threads:**

| Thread | Description |
|--------|-------------|
| `fuel_thread` | Fills fuel from 0% to 100% in random steps of 1–8 per second |
| `pressure_thread` | Fills pressure from 0 to 500 PSI in random steps of 10–40 per second |
| `temp_thread` | Fills temperature from 0 to 500°C in random steps of 10–40 per second |

All three threads run in parallel. Each prints a new line per update. When all three reach maximum, the child sends `FILL_DONE` to the parent via message queue and exits.

---

### 3. Control Tool — `control.c` (Second Terminal)

A small utility that sends operator commands through the Named FIFO.

**Accepted Commands:**

| Command | Effect |
|---------|--------|
| `start` | Begin filling fuel, pressure, and temperature |
| `fire` | Launch rocket (only valid after checking phase) |
| `abort` | Emergency stop at any point |
| `status` | Print current sensor values to terminal |

---

### 4. IPC Manager + Logger — `ipc.c`

Owns all IPC resource creation, destruction, the log file writer, and the `log_msg()` console print function.

**Log file:** `/tmp/rocket_sim.log`

All events are timestamped and written using `open`, `write`, and `close`.

---

## File Structure

```
RocketSim/
├── include/
│   └── common.h        ← shared structs, IPC keys, all prototypes
├── src/
│   ├── main.c          ← parent process: state machine, FIFO listener, countdown, flight
│   ├── sensor.c        ← child process: 3 pthreads fill fuel/pressure/temp simultaneously
│   └── ipc.c           ← IPC helpers, logger, global flags
├── tools/
│   └── control.c       ← operator command tool (run in second terminal)
├── Makefile
├── README.md
└── LICENSE
```

---

## Technologies Used

| Component | Technology |
|-----------|------------|
| Language | C (C99) |
| Build System | GNU Make |
| Threading | POSIX pthreads |
| IPC | Shared Memory, Message Queue, FIFO, Pipe, Semaphore |
| Synchronization | POSIX Semaphore, pthread Mutex |
| Signal Handling | `sigaction` |
| Logging | File I/O — `open` / `write` / `close` |
| Platform | Linux (Ubuntu 20.04+) |

---

## IPC Mechanisms Used

| # | Mechanism | Purpose |
|---|-----------|---------|
| 1 | **Unnamed Pipe** | Parent sends `shm_id` + `msg_id` to child after `fork()` |
| 2 | **Shared Memory** | Stores live sensor values — fuel, pressure, temp, speed, altitude |
| 3 | **POSIX Semaphore** | Protects shared memory from simultaneous access |
| 4 | **System V Message Queue** | Child notifies parent when all sensors are fully filled |
| 5 | **Named FIFO** | `control` tool sends operator commands to the parent process |

---

## System Calls Used

| System Call | Purpose |
|-------------|---------|
| `fork()` | Create the sensor child process |
| `waitpid()` | Reap child on shutdown — prevents zombie processes |
| `pipe()` | Pass `shm_id` and `msg_id` from parent to child |
| `mkfifo()` | Create named FIFO for operator commands |
| `shmget()` | Create shared memory segment |
| `shmat()` | Attach shared memory into process address space |
| `shmdt()` | Detach shared memory |
| `shmctl()` | Destroy shared memory segment |
| `msgget()` | Create System V message queue |
| `msgsnd()` | Child sends FILL_DONE to parent |
| `msgrcv()` | Parent receives FILL_DONE from child |
| `msgctl()` | Destroy message queue |
| `sem_open()` | Create POSIX named semaphore |
| `sem_wait()` | Lock semaphore before shared memory access |
| `sem_post()` | Unlock semaphore after shared memory access |
| `sem_unlink()` | Remove named semaphore |
| `pthread_create()` | Create fuel, pressure, and temp threads |
| `pthread_join()` | Wait for all threads to complete |
| `pthread_mutex_lock()` | Protect console output from interleaved printing |
| `sigaction()` | Install SIGINT, SIGTERM, SIGUSR1 handlers |
| `kill()` | Send SIGTERM to child on abort |
| `open()` | Create and open log file |
| `write()` | Write timestamped log entries |
| `close()` | Close log file on shutdown |
| `time()` | Get current timestamp for log entries |

---

## Signal Handling

| Signal | Source | Effect |
|--------|--------|--------|
| `SIGINT` | Ctrl+C | Graceful shutdown — stops threads, kills child, frees all IPC |
| `SIGTERM` | Parent → child | Stops all sensor threads inside child process |
| `SIGUSR1` | Child → parent | Signals an abort condition from inside the child |
| `SIGPIPE` | Broken FIFO | Ignored — prevents crash when pipe disconnects |

---

## Requirements

- Ubuntu 20.04 or later (any Linux distribution)
- GCC compiler
- Make

Install if not already present:

```bash
sudo apt update
sudo apt install gcc make -y
```

---

## Build Instructions

```bash
git clone https://github.com/YOUR_USERNAME/RocketSim.git
cd RocketSim
make all
```

Produces two executables:
- `rocket_sim` — the main simulation
- `control` — the operator command tool

---

## Run the System

### Terminal 1 — Start the simulation

```bash
./rocket_sim
```

### Terminal 2 — Send commands

```bash
./control start
./control status
./control fire
./control abort
```

---

## Sample Output

```
Filling rocket systems...

FUEL        5.0 %
PRESSURE    30 PSI
TEMP        22 C
FUEL        12.0 %
TEMP        58 C
PRESSURE    64 PSI
FUEL        100.0 % -- TANK FULL
PRESSURE    500 PSI -- MAX REACHED
TEMP        500 C -- MAX REACHED

Checking all systems
CHECK  5 ...
CHECK  4 ...
CHECK  3 ...
CHECK  2 ...
CHECK  1 ...
CHECK  All systems GO

Rocket is ready.  Type:  fire

CMD  FIRE received

LAUNCH COUNTDOWN
COUNTDOWN  10
COUNTDOWN  9
...
COUNTDOWN  1
IGNITION !

ROCKET LAUNCHED

SPEED       187 m/s
ALTITUDE    212 m
SPEED       421 m/s
ALTITUDE    847 m
```

---

## Log File

All events are written to `/tmp/rocket_sim.log` with timestamps:

```
[10:21:42] Simulation started
[10:21:44] CMD: start
[10:21:45] Filling started
[10:21:45] FUEL 5.0%
[10:21:45] PRESSURE 30 PSI
[10:21:46] TEMP 22 C
[10:22:10] FUEL FULL
[10:22:10] PRESSURE MAX
[10:22:10] TEMP MAX
[10:22:11] Checking phase started
[10:22:16] Check complete – all GO
[10:22:18] CMD: fire
[10:22:19] Countdown started
[10:22:29] IGNITION
[10:22:30] Speed 187 m/s  Altitude 212 m
[10:22:35] ABORTED
[10:22:35] Final – Fuel 100.0%  Pressure 500  Temp 500  Speed 421  Alt 2451
```

---

## Testing Scenarios

### Normal Launch
- Send `start` → all three sensors fill to maximum
- Wait for checking phase to complete (5 seconds)
- Send `fire` → countdown from 10 to 1 → IGNITION
- Speed and altitude increase every second

### Abort During Filling
- Send `start` → sensors begin filling simultaneously
- Send `abort` while filling is still in progress
- Simulation stops immediately and all IPC resources are freed

### Abort During Countdown
- Send `start` → wait for check to complete
- Send `fire` → countdown begins
- Send `abort` during countdown → simulation halts immediately

### Status Check
- Send `status` at any point during the simulation
- Prints current fuel, pressure, temp, speed, altitude, and state

---

## Demo Steps

**Step 1 — Build**
```bash
make all
```

**Step 2 — Run in Terminal 1**
```bash
./rocket_sim
```

**Step 3 — Send commands from Terminal 2**
```bash
./control start
./control status
./control fire
./control abort
```

**Step 4 — Read the log**
```bash
cat /tmp/rocket_sim.log
```

**Step 5 — Shutdown**
```
Ctrl + C
```

**Step 6 — Clean build files**
```bash
make clean
```

---

## Makefile Targets

```bash
make all     # compile everything
make run     # build and run rocket_sim
make clean   # remove binaries, object files, and log files
```

---

## Reflection

### What Worked
- All 5 IPC mechanisms working together seamlessly
- Three sensor threads filling simultaneously without output corruption — protected by `pthread_mutex`
- Clean shutdown with zero zombie processes using `waitpid()`
- Real-time operator command interface via Named FIFO from a second terminal
- Log file correctly timestamped and written for every event

### Challenges Faced
- **Interleaved thread output** — three threads printing at the same time caused garbled output; fixed by protecting all `printf` calls with a shared `pthread_mutex`
- **Pipe before fork** — creating pipes after `fork()` caused "bad file descriptor" errors in the child; fixed by creating all pipes before the first `fork()` call
- **FIFO blocking on open** — opening the FIFO with `O_RDONLY` blocked until a writer connected; fixed by opening with `O_RDWR` so the process is its own writer and read never returns EOF

### What I Learned
- Linux process management using `fork` and `waitpid`
- Thread synchronization using `pthread_mutex`
- All 5 IPC mechanisms — Shared Memory, Semaphore, Message Queue, Named FIFO, Unnamed Pipe
- Signal handling using `sigaction`
- File I/O logging using `open`, `write`, and `close`
- Designing a clean state machine that responds to asynchronous operator commands in real time

---

## Author

**Your Name**
- GitHub: [@YOUR_USERNAME](https://github.com/YOUR_USERNAME)

---

## License

This project is open source and available under the [MIT License](LICENSE).
