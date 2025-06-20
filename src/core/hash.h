
#ifndef HASH_H
#define HASH_H
#define HASH_SIZE 64
void sha256_hash(const char* content, size_t size, char* hash_out);
void sha256_hash_object(const char* type, const char* content, size_t size, char* hash_out);
#endif //HASH_H
