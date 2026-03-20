/*
 * sensor.c  –  Sensor child process
 *
 * Spawns 3 threads that fill fuel, pressure, temp simultaneously.
 * Each thread prints a new line for every update.
 * When all three are full, sends a message to the parent and exits.
 *
 * IPC used:
 *   Shared memory  – writes fuel, pressure, temp
 *   Semaphore      – protects shared memory writes
 *   Message queue  – notifies parent when fill is complete
 */
#include "../include/common.h"

typedef struct {
    SharedData *shm;
    sem_t      *sem;
    int         msg_id;
} SensorCtx;

static int fuel_done = 0;
static int pres_done = 0;
static int temp_done = 0;
static pthread_mutex_t done_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Helper: check if all three are done ─────────────── */
static void check_all_done(SensorCtx *ctx) {
    pthread_mutex_lock(&done_lock);
    int all = fuel_done && pres_done && temp_done;
    pthread_mutex_unlock(&done_lock);

    if (all) {
        /* Mark fill complete in shared memory */
        sem_wait(ctx->sem);
        ctx->shm->fill_done = 1;
        ctx->shm->state     = STATE_CHECKING;
        sem_post(ctx->sem);

        /* Notify parent via message queue */
        Msg m; m.mtype = 1;
        strncpy(m.text, "FILL_DONE", sizeof(m.text));
        msgsnd(ctx->msg_id, &m, sizeof(m)-sizeof(long), 0);
    }
}

/* ════════ THREAD: fuel ═══════════════════════════════
 * Fills fuel from 0 to 100% in steps of rand(1..8)
 */
static void *fuel_thread(void *arg) {
    SensorCtx *ctx = (SensorCtx*)arg;
    srand((unsigned)(time(NULL) ^ getpid() ^ 1));

    while (!g_abort) {
        sem_wait(ctx->sem);
        double cur = ctx->shm->fuel;
        sem_post(ctx->sem);

        if (cur >= 100.0) break;

        double step = (rand() % 8) + 1;   /* 1 – 8 */
        double next = cur + step;
        if (next > 100.0) next = 100.0;

        sem_wait(ctx->sem);
        ctx->shm->fuel = next;
        sem_post(ctx->sem);

        pthread_mutex_lock(&g_print_lock);
        printf("FUEL        %.1f %%\n", next);
        fflush(stdout);
        pthread_mutex_unlock(&g_print_lock);

        log_write("FUEL %.1f%%", next);

        if (next >= 100.0) {
            pthread_mutex_lock(&g_print_lock);
            printf("FUEL        100.0 %% -- TANK FULL\n");
            fflush(stdout);
            pthread_mutex_unlock(&g_print_lock);
            log_write("FUEL FULL");
            break;
        }
        sleep(1);
    }

    pthread_mutex_lock(&done_lock);
    fuel_done = 1;
    pthread_mutex_unlock(&done_lock);
    check_all_done(ctx);
    return NULL;
}

/* ════════ THREAD: pressure ════════════════════════════
 * Fills pressure from 0 to 500 in steps of rand(10..40)
 */
static void *pressure_thread(void *arg) {
    SensorCtx *ctx = (SensorCtx*)arg;
    srand((unsigned)(time(NULL) ^ getpid() ^ 2));

    while (!g_abort) {
        sem_wait(ctx->sem);
        double cur = ctx->shm->pressure;
        sem_post(ctx->sem);

        if (cur >= 500.0) break;

        double step = (rand() % 31) + 10;   /* 10 – 40 */
        double next = cur + step;
        if (next > 500.0) next = 500.0;

        sem_wait(ctx->sem);
        ctx->shm->pressure = next;
        sem_post(ctx->sem);

        pthread_mutex_lock(&g_print_lock);
        printf("PRESSURE    %.0f PSI\n", next);
        fflush(stdout);
        pthread_mutex_unlock(&g_print_lock);

        log_write("PRESSURE %.0f PSI", next);

        if (next >= 500.0) {
            pthread_mutex_lock(&g_print_lock);
            printf("PRESSURE    500 PSI -- MAX REACHED\n");
            fflush(stdout);
            pthread_mutex_unlock(&g_print_lock);
            log_write("PRESSURE MAX");
            break;
        }
        sleep(1);
    }

    pthread_mutex_lock(&done_lock);
    pres_done = 1;
    pthread_mutex_unlock(&done_lock);
    check_all_done(ctx);
    return NULL;
}

/* ════════ THREAD: temp ════════════════════════════════
 * Fills temperature from 0 to 500 in steps of rand(10..40)
 */
static void *temp_thread(void *arg) {
    SensorCtx *ctx = (SensorCtx*)arg;
    srand((unsigned)(time(NULL) ^ getpid() ^ 3));

    while (!g_abort) {
        sem_wait(ctx->sem);
        double cur = ctx->shm->temp;
        sem_post(ctx->sem);

        if (cur >= 500.0) break;

        double step = (rand() % 31) + 10;   /* 10 – 40 */
        double next = cur + step;
        if (next > 500.0) next = 500.0;

        sem_wait(ctx->sem);
        ctx->shm->temp = next;
        sem_post(ctx->sem);

        pthread_mutex_lock(&g_print_lock);
        printf("TEMP        %.0f C\n", next);
        fflush(stdout);
        pthread_mutex_unlock(&g_print_lock);

        log_write("TEMP %.0f C", next);

        if (next >= 500.0) {
            pthread_mutex_lock(&g_print_lock);
            printf("TEMP        500 C -- MAX REACHED\n");
            fflush(stdout);
            pthread_mutex_unlock(&g_print_lock);
            log_write("TEMP MAX");
            break;
        }
        sleep(1);
    }

    pthread_mutex_lock(&done_lock);
    temp_done = 1;
    pthread_mutex_unlock(&done_lock);
    check_all_done(ctx);
    return NULL;
}

/* ════════ Entry point ═════════════════════════════════ */
void sensor_run(int shm_id, int msg_id) {
    SharedData *shm;
    if (shm_attach(shm_id, &shm) < 0) exit(1);
    sem_t *sem = sem_get();

    SensorCtx ctx = { shm, sem, msg_id };

    printf("\nFilling rocket systems...\n\n");
    fflush(stdout);
    log_write("Filling started");

    /* Launch all 3 threads simultaneously */
    pthread_t t_fuel, t_pres, t_temp;
    pthread_create(&t_fuel, NULL, fuel_thread,     &ctx);
    pthread_create(&t_pres, NULL, pressure_thread, &ctx);
    pthread_create(&t_temp, NULL, temp_thread,     &ctx);

    pthread_join(t_fuel, NULL);
    pthread_join(t_pres, NULL);
    pthread_join(t_temp, NULL);

    sem_release(sem);
    shmdt(shm);
}
