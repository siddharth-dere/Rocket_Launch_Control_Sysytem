# 🚀 Rocket Launch Control System

A simulation of a rocket launch control system built entirely  using **Linux System Calls and POSIX APIs**. No hardware required — all sensor data is simulated using random values, timers, and shared memory.

---

## Project Overview

RocketSim simulates a real rocket launch sequence where a **parent process** acts as the Launch Director and a **child process** manages sensor filling (fuel, pressure, temperature). Commands are sent from a second terminal in real time using a Named FIFO pipe.

### Launch Sequence

```
start  →  Fill fuel (0–100%)  +  pressure (0–500 PSI)  +  temp (0–500°C)
       →  Checking phase (5 seconds)
fire   →  10-second countdown  →  IGNITION
       →  Speed and altitude update every second
abort  →  Stop simulation at any point
```

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
│   └── control.c       ← command sender tool (run in second terminal)
├── Makefile
└── README.md
```

---

## IPC Mechanisms Used

| # | Mechanism | Purpose |
|---|-----------|---------|
| 1 | **Unnamed Pipe** | Parent sends `shm_id` + `msg_id` to child after `fork()` |
| 2 | **Shared Memory** | Stores live sensor values (fuel, pressure, temp, speed, altitude) |
| 3 | **POSIX Semaphore** | Protects shared memory from simultaneous access |
| 4 | **System V Message Queue** | Child notifies parent when all sensors are fully filled |
| 5 | **Named FIFO** | `control` tool sends commands (start/fire/abort/status) to parent |

---

## System Calls Used

| Category | Calls |
|----------|-------|
| Process Control | `fork`, `waitpid`, `exit`, `getpid` |
| File I/O | `open`, `write`, `close`, `read` |
| IPC — Pipe | `pipe` |
| IPC — FIFO | `mkfifo`, `open`, `read`, `unlink` |
| IPC — Shared Memory | `shmget`, `shmat`, `shmdt`, `shmctl` |
| IPC — Message Queue | `msgget`, `msgsnd`, `msgrcv`, `msgctl` |
| IPC — Semaphore | `sem_open`, `sem_wait`, `sem_post`, `sem_close`, `sem_unlink` |
| Threads | `pthread_create`, `pthread_join`, `pthread_mutex_lock/unlock` |
| Signals | `sigaction`, `signal`, `kill` |
| Time | `time`, `localtime`, `strftime` |

---

## Requirements

- Ubuntu 20.04 or later (any Linux distro)
- GCC compiler
- Make

Install if not already present:

```bash
sudo apt update
sudo apt install gcc make -y
```

---

## Build

```bash
git clone https://github.com/YOUR_USERNAME/RocketSim.git
cd RocketSim
make all
```

This produces two executables:
- `rocket_sim` — the main simulation
- `control` — the command sender tool

---

## How to Run

### Terminal 1 — Start the simulation

```bash
./rocket_sim
```

You will see:
```
Rocket Simulation started.
Commands (from another terminal):  start  |  fire  |  abort  |  status
Waiting for command...
```

### Terminal 2 — Send commands

```bash
# Begin filling fuel, pressure, and temperature
./control start

# Check current values at any time
./control status

# Launch the rocket (only works after checking phase completes)
./control fire

# Emergency stop at any point
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
...
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

All events are logged to `/tmp/rocket_sim.log` with timestamps:

```
[10:21:42] Simulation started
[10:21:44] CMD: start
[10:21:45] Filling started
[10:21:45] FUEL 5.0%
[10:21:45] PRESSURE 30 PSI
[10:21:46] TEMP 22 C
...
[10:22:10] FUEL FULL
[10:22:10] Check complete – all GO
[10:22:15] CMD: fire
[10:22:16] Countdown started
[10:22:26] IGNITION
[10:22:27] Speed 187 m/s  Altitude 212 m
```

---

## Makefile Targets

```bash
make all     # build everything
make clean   # remove binaries and log files
make run     # build and run rocket_sim
```

---

## How It Works (Architecture)

```
Terminal 2                     Terminal 1
./control start ──FIFO──►  main.c  (parent process)
./control fire  ──FIFO──►    │
./control abort ──FIFO──►    │  fork()
./control status──FIFO──►    │
                             │
                        pipe(shm_id, msg_id)
                             │
                             ▼
                        sensor.c  (child process)
                          ├── fuel_thread     (0→100%,  step 1–8)
                          ├── pressure_thread (0→500,   step 10–40)
                          └── temp_thread     (0→500,   step 10–40)
                             │
                        msgq: FILL_DONE
                             │
                             ▼
                        Checking (5s) → Countdown (10s) → Flight loop
                             │
                        SharedData ◄──► semaphore
                        (fuel, pressure, temp, speed, altitude)
```

---

## Author


- GitHub: @siddharth-dere

---


