//
// Created by Atheria on 6/20/25.
//

#ifndef OBJECTS_H
#define OBJECTS_H
int store_object(const char* type, const char* content, size_t size, char* hash_out);
// Store a blob object directly from a file on disk, streaming to avoid reading the full file into memory.
int store_blob_from_file(const char* filepath, char* hash_out);
// Compute SHA-256 of file and output hex string (64 chars + null). Returns 0 on success, -1 on error.
int sha256_file_hex(const char* filepath, char hash_out[65]);
char* load_object(const char* hash, size_t* size_out, char* type_out);
#endif //OBJECTS_H
