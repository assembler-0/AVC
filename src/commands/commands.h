//
// Created by Atheria on 6/20/25.
//

#ifndef COMMANDS_H
#define COMMANDS_H
#include <stddef.h>
// Command function declarations
int cmd_init(int argc, char* argv[]);
int cmd_add(int argc, char* argv[]);
int cmd_commit(int argc, char* argv[]);
int cmd_status(int argc, char* argv[]);
int cmd_log(int argc, char* argv[]);
int cmd_rm(int argc, char* argv[]);
int cmd_reset(int argc, char* argv[]);
int cmd_version(int argc, char* argv[]);
int cmd_clean(int argc, char* argv[]);
int cmd_agcl(int argc, char* argv[]);

#endif
