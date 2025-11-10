/**
 * nonstop_networking
 * CS 241 - Spring 2022
 */
#pragma once
#include <stddef.h>
#include <sys/types.h>

#define LOG(...)                      \
    do {                              \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
    } while (0);

typedef enum { GET, PUT, DELETE, LIST, V_UNKNOWN } verb;


size_t write_all_to_socket(int socket, const char *buffer, size_t count);
size_t read_all_from_socket(int socket, char *buffer, size_t count);
size_t get_message_size(int socket);
size_t write_message_size(size_t size, int socket);
size_t server_write_all_to_socket(int socket, const char *buffer, size_t count, int* flag);
size_t server_read_all_from_socket(int socket, char *buffer, size_t count, int* flag);
size_t server_read_all_header(int socket, char *buffer, size_t count, int* flag);
