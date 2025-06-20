//
// Created by Atheria on 6/20/25.
//

#ifndef OBJECTS_H
#define OBJECTS_H
int store_object(const char* type, const char* content, size_t size, char* hash_out);
char* load_object(const char* hash, size_t* size_out, char* type_out);
#endif //OBJECTS_H
