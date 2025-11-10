/**
 * nonstop_networking
 * CS 241 - Spring 2022
 */
#include <stddef.h>
#include <sys/types.h>
#include "common.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "format.h"


size_t get_message_size(int socket) {
    size_t size;
    size_t read_bytes =
        read_all_from_socket(socket, (char *)&size, sizeof(size_t));
    if (read_bytes == 0 || (int)read_bytes == -1) {
        print_invalid_response();
        exit(1);
    }

    return (size_t)(size);
}

/**
 * Writes the bytes of size to the socket
 *
 * Returns the number of bytes successfully written,
 * 0 if socket is disconnected, or -1 on failure
 */
// You may assume size won't be larger than a 4 byte integer
size_t write_message_size(size_t size, int socket) {
    // Your code here
    size_t write_bytes =
        write_all_to_socket(socket, (char *)&size, sizeof(size_t));
    return write_bytes;
}


/**
 * Attempts to read all count bytes from socket into buffer.
 * Assumes buffer is large enough.
 *
 * Returns the number of bytes read, 0 if socket is disconnected,
 * or -1 on failure.
 */
size_t read_all_from_socket(int socket, char *buffer, size_t count) {
    // Your Code Here
    size_t num_read = 0;
    while(num_read < count) {
        ssize_t ret = read(socket, buffer, count-num_read);
        if (ret == 0) return num_read;
        if (ret == -1 && errno == EINTR) {
            continue;
        }
        if (ret == -1) return -1;
        num_read += ret;
        buffer += ret;
    }
    return num_read;
}

/**
 * Attempts to write all count bytes from buffer to socket.
 * Assumes buffer contains at least count bytes.
 *
 * Returns the number of bytes written, 0 if socket is disconnected,
 * or -1 on failure.
 */
size_t write_all_to_socket(int socket, const char *buffer, size_t count) {
    // Your Code Here
    size_t num_write = 0;
    while(num_write < count) {
        ssize_t ret = write(socket, buffer, count-num_write);
        if (ret == 0) return num_write;
        if (ret == -1 && errno == EINTR) {
            continue;
        }
        if (ret == -1) return -1;
        num_write += ret;
        buffer += ret;
    }
    return num_write;
}



size_t server_read_all_header(int socket, char *buffer, size_t count, int* flag) {
    // Your Code Here
    size_t num_read = 0;
    while(num_read < count) {
        ssize_t ret = read(socket, buffer, 1);
        if (ret == 0) { /*meaning reach 1024 but still not find '\n'*/
            *flag = -1; 
            return num_read;
        }
        if (ret == -1 &&  (errno == EAGAIN || errno == EWOULDBLOCK)) return num_read;
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1) return -1;

        //check for '\n'
        if (buffer[0] == '\n') {
            *flag = 1;
            // Do not store the newlines in your filenames & make it end with '\0'
            buffer[0] = '\0';
            return num_read;
        }

        //update value
        num_read += ret;
        buffer += ret;
    }
    
    if (num_read > count) {
        *flag = -1;
    }
    
    return num_read;
}



size_t server_read_all_from_socket(int socket, char *buffer, size_t count, int* flag) {
    // Your Code Here
    size_t num_read = 0;
    while(num_read < count) {
        ssize_t ret = read(socket, buffer, count-num_read);
        if (ret == 0) {
            *flag = 1;
            return num_read;
        }
        if (ret == -1 &&  (errno == EAGAIN || errno == EWOULDBLOCK)) return num_read;
        if (ret == -1 && errno == EINTR) {
            continue;
        }
        if (ret == -1) return -1;
        num_read += ret;
        buffer += ret;
    }
    return num_read;
}


size_t server_write_all_to_socket(int socket, const char *buffer, size_t count, int* flag) {
    // Your Code Here
    size_t num_write = 0;
    while(num_write < count) {
        ssize_t ret = write(socket, buffer, count-num_write);
        if (ret == 0) {
            *flag = 1;
            return num_write;
        }
        if (ret == -1 && errno == EPIPE) {
            *flag = 1;
            return num_write;
        }
        if (ret == -1 &&  (errno == EAGAIN || errno == EWOULDBLOCK)) return num_write;
        if (ret == -1 && errno == EINTR) {
            continue;
        }
        if (ret == -1) return -1;
        num_write += ret;
        buffer += ret;
    }
    return num_write;
}

