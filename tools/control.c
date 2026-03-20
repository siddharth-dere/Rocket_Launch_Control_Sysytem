/*
 * control.c  –  Command tool (run in a second terminal)
 *
 * Usage:
 *   ./control start
 *   ./control fire
 *   ./control abort
 *   ./control status
 */
#include "../include/common.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <start|fire|abort|status>\n", argv[0]);
        return 1;
    }

    /* Validate command */
    const char *valid[] = { "start", "fire", "abort", "status", NULL };
    int ok = 0;
    for (int i = 0; valid[i]; i++)
        if (!strcmp(argv[1], valid[i])) { ok = 1; break; }

    if (!ok) {
        printf("Unknown command '%s'\n", argv[1]);
        printf("Valid commands: start  fire  abort  status\n");
        return 1;
    }

    /* Open FIFO and send command */
    int fd = open(CMD_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("Cannot open FIFO – is rocket_sim running?");
        return 1;
    }

    Command cmd;
    memset(&cmd, 0, sizeof(cmd));
    strncpy(cmd.cmd, argv[1], sizeof(cmd.cmd) - 1);

    if (write(fd, &cmd, sizeof(cmd)) != (ssize_t)sizeof(cmd)) {
        perror("write to FIFO failed");
        close(fd);
        return 1;
    }

    close(fd);
    printf("Sent: %s\n", cmd.cmd);
    return 0;
}
