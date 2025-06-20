#include <stdio.h>
#include <string.h>
#include "commands/commands.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: avc <command> [args]\n");
        printf("Commands: init, add, commit, status, log\n");
        return 1;
    }

    char* command = argv[1];

    if (strcmp(command, "init") == 0) {
        return cmd_init(argc - 1, argv + 1);
    } else if (strcmp(command, "add") == 0) {
        return cmd_add(argc - 1, argv + 1);
    } else if (strcmp(command, "commit") == 0) {
        return cmd_commit(argc - 1, argv + 1);
    } else if (strcmp(command, "status") == 0) {
        return cmd_status(argc - 1, argv + 1);
    } else if (strcmp(command, "log") == 0) {
        return cmd_log(argc - 1, argv + 1);
    } else if (strcmp(command, "rm") == 0) {
        return cmd_rm(argc - 1, argv + 1);
    } else if (strcmp(command, "reset") == 0) {
        return cmd_reset(argc - 1, argv + 1);
    } else {
        printf("Unknown command: %s\n", command);
        return 1;
    }
}

