#ifndef COMMANDS_H
#define COMMANDS_H
#include <stddef.h>
// Command function declarations
int cmd_init(int argc, char* argv[]);
int cmd_add(int argc, char* argv[]);
int cmd_commit(int argc, char* argv[]);
int cmd_status(int argc, char* argv[]);
int cmd_log(int argc, char* argv[]);
int check_repo();
char* read_file(const char* filepath, size_t* size);
void simple_hash(const char* content, size_t size, char* hash_out);
void sha1_hash(const char* content, size_t size, char* hash_out);
void sha1_hash_object(const char* type, const char* content, size_t size, char* hash_out);
int store_object(const char* type, const char* content, size_t size, char* hash_out);
char* load_object(const char* hash, size_t* size_out, char* type_out);
#endif