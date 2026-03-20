/*
 * main.c  –  Rocket Simulation  (parent process)
 *
 * Flow:
 *   wait for "start" command via FIFO
 *     → fork sensor child (fills fuel/pressure/temp via 3 threads)
 *     → when fill done: checking phase (5 seconds)
 *     → print "Ready. Type: fire"
 *   wait for "fire" command
 *     → 10-second countdown
 *     → print speed + altitude every second
 *   "abort" at any time stops everything
 *   "status" prints current values
 *
 * IPC used:
 *   Named FIFO    – receives commands from control tool
 *   Shared memory – live sensor data
 *   Semaphore     – protects shared memory
 *   Message queue – sensor child notifies parent when filling done
 *   Unnamed pipe  – parent sends shm_id/msg_id to child
 *   SIGUSR1       – abort signal path
 *   SIGALRM       – used for countdown timing
 */
#include "../include/common.h"

/* ── Signal handlers ──────────────────────────────────── */
static void h_abort(int s)    { (void)s; g_abort    = 1; }
static void h_shutdown(int s) { (void)s; g_shutdown = 1; }

/* ── Globals ──────────────────────────────────────────── */
static int         shm_id, msg_id;
static SharedData *shm;
static sem_t      *sem;
static pid_t       sensor_pid = -1;

/* ── Safe print: one line at a time ──────────────────── */
static void say(const char *fmt, ...) {
    pthread_mutex_lock(&g_print_lock);
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&g_print_lock);
}

/* ── Print current sensor values ─────────────────────── */
static void print_status(void) {
    sem_wait(sem);
    double f = shm->fuel, p = shm->pressure,
           t = shm->temp, sp = shm->speed, al = shm->altitude;
    int st = shm->state;
    sem_post(sem);

    const char *stnames[] = {
        "IDLE","FILLING","CHECKING","READY","COUNTDOWN","FLYING","ABORTED"
    };
    say("STATUS");
    say("Fuel     : %.1f %%",  f);
    say("Pressure : %.0f PSI", p);
    say("Temp     : %.0f C",   t);
    say("Speed    : %.1f m/s", sp);
    say("Altitude : %.1f m",   al);
    say("State    : %s", (st >= 0 && st <= 6) ? stnames[st] : "?");
    say("......");
}

/* ── Fork the sensor child ────────────────────────────── */
static void start_sensor(void) {
    /* Use an unnamed pipe to pass IDs to the child */
    int pfd[2];
    CHECK(pipe(pfd), "pipe");

    sensor_pid = fork();
    CHECK(sensor_pid, "fork");

    if (sensor_pid == 0) {
        /* ── Child ── */
        close(pfd[1]);
        int ids[2];
        if (read(pfd[0], ids, sizeof(ids)) != sizeof(ids))
            { perror("child pipe read"); exit(1); }
        close(pfd[0]);
        sensor_run(ids[0], ids[1]);
        exit(0);
    }

    /* ── Parent ── */
    close(pfd[0]);
    int ids[2] = { shm_id, msg_id };
    write(pfd[1], ids, sizeof(ids));
    close(pfd[1]);
}

/* ── Checking phase (5 seconds) ──────────────────────── */
static void run_checking(void) {
    say("\nChecking all systems");
    log_write("Checking phase started");
    for (int i = 5; i >= 1 && !g_abort; i--) {
        say("CHECK  %d ...", i);
        sleep(1);
    }
    if (!g_abort) {
        say("CHECK  All systems GO");
        say("\nRocket is ready.  Type:  fire");
        log_write("Check complete – all GO");
        sem_wait(sem);
        shm->state = STATE_READY;
        sem_post(sem);
    }
}

/* ── Countdown (10 seconds) ──────────────────────────── */
static void run_countdown(void) {
    say("\nLAUNCH COUNTDOWN");
    log_write("Countdown started");
    sem_wait(sem);
    shm->state = STATE_COUNTDOWN;
    sem_post(sem);

    for (int i = 10; i >= 1 && !g_abort; i--) {
        say("COUNTDOWN  %d", i);
        sleep(1);
    }
    if (g_abort) return;

    say("IGNITION !");
    log_write("IGNITION");

    sem_wait(sem);
    shm->state = STATE_FLYING;
    sem_post(sem);
}

/* ── Flight loop: speed + altitude every second ──────── */
static void run_flight(void) {
    say("\nROCKET LAUNCHED\n");
    log_write("Flight started");

    srand((unsigned)time(NULL));
    double speed    = 0.0;
    double altitude = 0.0;

    while (!g_abort && !g_shutdown) {
        /* Speed: increases by random 50–200 m/s per second */
        double ds = (rand() % 151) + 50;
        speed    += ds;

        /* Altitude: increases by current speed × 1 second */
        double da = speed + ((rand() % 51) - 25);
        altitude += da;

        sem_wait(sem);
        shm->speed    = speed;
        shm->altitude = altitude;
        sem_post(sem);

        say("SPEED       %.0f m/s", speed);
        say("ALTITUDE    %.0f m",   altitude);

        log_write("Speed %.0f m/s  Altitude %.0f m", speed, altitude);
        sleep(1);
    }
}

/* ── FIFO listener thread ─────────────────────────────── */
static volatile int fire_requested = 0;
static volatile int start_requested = 0;

static void *fifo_thread(void *arg) {
    (void)arg;
    /* O_RDWR keeps FIFO alive even with no writer */
    int fd = open(CMD_FIFO, O_RDWR);
    if (fd < 0) { perror("open(FIFO)"); return NULL; }

    Command cmd;
    while (!g_shutdown) {
        ssize_t r = read(fd, &cmd, sizeof(cmd));
        if (r == (ssize_t)sizeof(cmd)) {

            if (!strcmp(cmd.cmd, "start")) {
                sem_wait(sem);
                int st = shm->state;
                sem_post(sem);
                if (st == STATE_IDLE) {
                    start_requested = 1;
                    say("\nCMD  START received");
                    log_write("CMD: start");
                } else {
                    say("CMD  Already started");
                }

            } else if (!strcmp(cmd.cmd, "fire")) {
                sem_wait(sem);
                int st = shm->state;
                sem_post(sem);
                if (st == STATE_READY) {
                    fire_requested = 1;
                    say("\nCMD  FIRE received");
                    log_write("CMD: fire");
                } else if (st == STATE_IDLE || st == STATE_FILLING) {
                    say("CMD  Not ready yet – run start first");
                } else if (st == STATE_CHECKING) {
                    say("CMD  Still checking – please wait");
                } else {
                    say("CMD  Already launched or aborted");
                }

            } else if (!strcmp(cmd.cmd, "abort")) {
                say("\nCMD  ABORT received !");
                log_write("CMD: abort");
                g_abort = 1;
                if (sensor_pid > 0) kill(sensor_pid, SIGTERM);

            } else if (!strcmp(cmd.cmd, "status")) {
                print_status();

            } else {
                say("CMD  Unknown command: %s", cmd.cmd);
            }

        } else if (r < 0 && errno == EINTR) {
            continue;
        } else if (r == 0) {
            break;
        }
    }
    close(fd);
    return NULL;
}

/* ═══════════════════════ main ════════════════════════ */
int main(void) {
    /* Clean up any stale IPC from previous run */
    ipc_cleanup_all();

    /* Signal handlers */
    struct sigaction sa; memset(&sa, 0, sizeof(sa)); sigemptyset(&sa.sa_mask);
    sa.sa_handler = h_abort;    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = h_shutdown; sigaction(SIGINT,  &sa, NULL);
                                sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_DFL);

    /* Create IPC */
    shm_id = shm_create(&shm);
    msg_id = msgq_create();
    sem    = sem_make();
    fifo_make();
    log_init();

    shm->state   = STATE_IDLE;
    shm->sim_pid = getpid();

    say("Rocket Simulation started.");
    say("Commands (from another terminal):  start  |  fire  |  abort  |  status");
    say("Waiting for command...\n");
    log_write("Simulation started");

    /* Start FIFO listener thread */
    pthread_t t_fifo;
    pthread_create(&t_fifo, NULL, fifo_thread, NULL);

    /* ── Main state machine ────────────────────────────── */
    while (!g_shutdown && !g_abort) {

        /* 1. Wait for start command */
        if (!start_requested) { sleep(1); continue; }
        start_requested = 0;

        sem_wait(sem);
        shm->state = STATE_FILLING;
        sem_post(sem);

        /* 2. Fork sensor child – fills fuel/pressure/temp */
        start_sensor();

        /* 3. Wait for fill_done message from child */
        Msg m;
        while (!g_abort) {
            if (msgrcv(msg_id, &m, sizeof(m)-sizeof(long), 1, IPC_NOWAIT) > 0) {
                if (!strcmp(m.text, "FILL_DONE")) break;
            }
            sleep(1);
        }
        if (g_abort) break;

        /* Collect sensor child */
        int status;
        waitpid(sensor_pid, &status, 0);
        sensor_pid = -1;

        /* 4. Checking phase */
        run_checking();
        if (g_abort) break;

        /* 5. Wait for fire command */
        while (!fire_requested && !g_abort && !g_shutdown)
            sleep(1);
        if (g_abort) break;
        fire_requested = 0;

        /* 6. Countdown */
        run_countdown();
        if (g_abort) break;

        /* 7. Flight */
        run_flight();
        break;
    }

    /* ── Shutdown ──────────────────────────────────────── */
    if (g_abort) {
        sem_wait(sem);
        shm->state = STATE_ABORTED;
        sem_post(sem);
        say("\nABORT  simulation stopped");
        log_write("ABORTED");
    } else {
        say("\nSimulation ended.");
        log_write("Simulation ended");
    }

    pthread_cancel(t_fifo);
    pthread_join(t_fifo, NULL);

    if (sensor_pid > 0) {
        kill(sensor_pid, SIGTERM);
        waitpid(sensor_pid, NULL, 0);
    }

    log_write("Final – Fuel %.1f%%  Pressure %.0f  Temp %.0f  Speed %.0f  Alt %.0f",
              shm->fuel, shm->pressure, shm->temp, shm->speed, shm->altitude);

    log_close();
    fifo_nuke();
    msgq_destroy(msg_id);
    sem_nuke(sem);
    shm_destroy(shm_id, shm);
    return 0;
}
