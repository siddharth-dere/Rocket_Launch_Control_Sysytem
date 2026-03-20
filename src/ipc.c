#include "../include/common.h"

/* ── Global flags ────────────────────────────────────── */
volatile sig_atomic_t g_abort    = 0;
volatile sig_atomic_t g_shutdown = 0;
pthread_mutex_t       g_print_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Log file fd ─────────────────────────────────────── */
static int log_fd = -1;

/* ════════════ Shared Memory ════════════════════════════ */
int shm_create(SharedData **out) {
    int old = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (old >= 0) shmctl(old, IPC_RMID, NULL);
    int id = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT|IPC_EXCL|0666);
    CHECK(id, "shmget");
    void *p = shmat(id, NULL, 0);
    if (p == (void*)-1) { perror("shmat"); exit(1); }
    memset(p, 0, sizeof(SharedData));
    *out = (SharedData*)p;
    return id;
}
int shm_attach(int id, SharedData **out) {
    void *p = shmat(id, NULL, 0);
    if (p == (void*)-1) { perror("shmat(child)"); return -1; }
    *out = (SharedData*)p; return 0;
}
void shm_destroy(int id, SharedData *s) { shmdt(s); shmctl(id, IPC_RMID, NULL); }

/* ════════════ Message Queue ════════════════════════════ */
int  msgq_create(void) {
    int old = msgget(MSG_KEY, 0666);
    if (old >= 0) msgctl(old, IPC_RMID, NULL);
    int id = msgget(MSG_KEY, IPC_CREAT|IPC_EXCL|0666);
    CHECK(id, "msgget"); return id;
}
void msgq_destroy(int id) { msgctl(id, IPC_RMID, NULL); }

/* ════════════ Semaphore ════════════════════════════════ */
sem_t *sem_make(void) {
    sem_unlink(SEM_NAME);
    sem_t *s = sem_open(SEM_NAME, O_CREAT|O_EXCL, 0666, 1);
    if (s == SEM_FAILED) { perror("sem_open"); exit(1); }
    return s;
}
sem_t *sem_get(void) {
    sem_t *s = sem_open(SEM_NAME, 0);
    if (s == SEM_FAILED) { perror("sem_get"); exit(1); }
    return s;
}
void sem_release(sem_t *s)  { sem_close(s); }
void sem_nuke(sem_t *s)     { sem_close(s); sem_unlink(SEM_NAME); }

/* ════════════ FIFO ═════════════════════════════════════ */
void fifo_make(void) {
    unlink(CMD_FIFO);
    if (mkfifo(CMD_FIFO, 0666) < 0 && errno != EEXIST)
        { perror("mkfifo"); exit(1); }
}
void fifo_nuke(void) { unlink(CMD_FIFO); }

/* ════════════ Full cleanup ═════════════════════════════ */
void ipc_cleanup_all(void) {
    int id;
    id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (id >= 0) shmctl(id, IPC_RMID, NULL);
    id = msgget(MSG_KEY, 0666);
    if (id >= 0) msgctl(id, IPC_RMID, NULL);
    sem_unlink(SEM_NAME);
    unlink(CMD_FIFO);
}

/* ════════════ Logger ═══════════════════════════════════ */
void log_init(void) {
    log_fd = open(LOG_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (log_fd < 0) perror("open(log)");
}

void log_write(const char *fmt, ...) {
    if (log_fd < 0) return;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char tbuf[20];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm);
    char line[256];
    int n = snprintf(line, sizeof(line), "[%s] ", tbuf);
    va_list ap; va_start(ap, fmt);
    n += vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    if (n < (int)sizeof(line) - 1) line[n++] = '\n';
    write(log_fd, line, n);
}

void log_close(void) {
    if (log_fd >= 0) { close(log_fd); log_fd = -1; }
}
