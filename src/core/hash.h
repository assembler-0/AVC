
#ifndef HASH_H
#define HASH_H
void sha1_hash(const char* content, size_t size, char* hash_out);
void sha1_hash_object(const char* type, const char* content, size_t size, char* hash_out);
#endif //HASH_H
