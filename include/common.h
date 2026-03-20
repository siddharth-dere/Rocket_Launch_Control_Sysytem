#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>

/* ── IPC ─────────────────────────────────────────────── */
#define SHM_KEY     0x2001
#define MSG_KEY     0x2002
#define SEM_NAME    "/rkt_sem"
#define CMD_FIFO    "/tmp/rkt_cmd"
#define LOG_FILE    "/tmp/rocket_sim.log"

/* ── Rocket states ───────────────────────────────────── */
#define STATE_IDLE      0
#define STATE_FILLING   1
#define STATE_CHECKING  2
#define STATE_READY     3
#define STATE_COUNTDOWN 4
#define STATE_FLYING    5
#define STATE_ABORTED   6

/* ── Shared memory layout ────────────────────────────── */
typedef struct {
    int    state;
    double fuel;        /* 0 – 100 % */
    double pressure;    /* 0 – 500   */
    double temp;        /* 0 – 500   */
    double speed;       /* m/s       */
    double altitude;    /* metres    */
    int    fill_done;   /* 1 = all three reached max */
    pid_t  sim_pid;     /* parent PID so child can signal it */
} SharedData;

/* ── System V message ────────────────────────────────── */
typedef struct {
    long mtype;
    char text[64];
} Msg;

/* ── FIFO command ────────────────────────────────────── */
typedef struct {
    char cmd[16];   /* "start" / "fire" / "abort" / "status" */
} Command;

/* ── Global flags (defined in ipc.c) ────────────────── */
extern volatile sig_atomic_t g_abort;
extern volatile sig_atomic_t g_shutdown;

/* ── Print mutex (defined in ipc.c) ─────────────────── */
extern pthread_mutex_t g_print_lock;

/* ── IPC helpers (ipc.c) ─────────────────────────────── */
int    shm_create(SharedData **out);
int    shm_attach(int id, SharedData **out);
void   shm_destroy(int id, SharedData *shm);
int    msgq_create(void);
void   msgq_destroy(int id);
sem_t *sem_make(void);
sem_t *sem_get(void);
void   sem_release(sem_t *s);
void   sem_nuke(sem_t *s);
void   fifo_make(void);
void   fifo_nuke(void);
void   ipc_cleanup_all(void);

/* ── Logger (ipc.c) ──────────────────────────────────── */
void log_init(void);
void log_write(const char *fmt, ...);
void log_close(void);

/* ── Sensor child entry point (sensor.c) ────────────── */
void sensor_run(int shm_id, int msg_id);

#define CHECK(rc,msg) do{if((rc)<0){perror(msg);exit(1);}}while(0)

#endif
