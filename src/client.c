/**
 * nonstop_networking
 * CS 241 - Spring 2022
 */
#include "format.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>

#include "common.h"


// ./client sp22-cs241-303.cs.illinois.edu:3456 PUT deeddddd.txt daddd.txt
// ./client sp22-cs241-303.cs.illinois.edu:3456 GET deed.txt break.txt

char **parse_args(int argc, char **argv);
verb check_args(char **args);

void write_to_server(char** cmds, verb operation, int socket_fd);
void read_from_server(char* local, verb operation, int socket_fd);
int connect_to_server(char *host, char *port);
int main(int argc, char **argv) {
    // Good luck!

    //parse args: in form of {host0, port1, method2, remote3, local4, NULL}
    char ** cmds = parse_args(argc, argv);  //need free

    //check args - inside will check for NULL of the argument
    verb operation = check_args(cmds);

    //connect to server
    int socket_fd = connect_to_server(cmds[0], cmds[1]);
    //write to server
    write_to_server(cmds, operation, socket_fd);

    //read from server
    read_from_server(cmds[4], operation, socket_fd);
    //close socket_fd
    close(socket_fd);
    
    //free
    free(cmds);
}


//{host0, port1, method2, remote3, local4, NULL}
void write_to_server(char** cmds, verb operation, int socket_fd) {
    char* msg = NULL;       //need free

    //1. write header to server
    switch(operation){
    case GET: {
        msg = calloc(1, strlen(cmds[2]) + strlen(cmds[3]) + 3); // ' ' & '\n' & '\0'
        sprintf(msg, "%s %s\n", cmds[2], cmds[3]);
        break;

    }
    case PUT: {
        msg = calloc(1, strlen(cmds[2]) + strlen(cmds[3]) + 3); // ' ' & '\n' & '\0'
        sprintf(msg, "%s %s\n", cmds[2], cmds[3]);
        break;
    }
    case DELETE: {
        msg = calloc(1, strlen(cmds[2]) + strlen(cmds[3]) + 3); // ' ' & '\n' & '\0'
        sprintf(msg, "%s %s\n", cmds[2], cmds[3]);
        break;
    }
    case LIST: {
        msg = calloc(1, strlen(cmds[2]) + 2); // '\n' &  '\0' 
        sprintf(msg, "%s\n", cmds[2]);
        break;
    }
    default: {/*should not reach here*/}
    }
    ssize_t write_ret = write_all_to_socket(socket_fd, msg, strlen(msg));
    if (write_ret == -1) exit(1);
    if (write_ret == 0) {
        print_connection_closed();
        exit(1);
    }

    //2. Only PUT have [File size][Binary Data]
    if (operation == PUT) {
        // requirement: Your client needs to be able to handle large files (more than physical RAM) and should do so efficiently.

        //open file - handle error - r: The stream is positioned at the beginning of the file.
        FILE* file = fopen(cmds[4], "r");   //need close
        if (!file) {
            //On PUT, if local file does not exist, simply exiting is okay.
            exit(1);
        }

        //write [size]
        struct stat file_info;
        stat(cmds[4], &file_info);
        size_t count = file_info.st_size;
        ssize_t msg_write_ret = write_message_size(count, socket_fd);
        if (msg_write_ret == -1) exit(1);
        if(msg_write_ret == 0) {
            print_connection_closed();
            exit(1);
        }

        //write [binary data]
        size_t num_write = 0;
        char buffer[4096+1];
        while(num_write < count) {
            memset(buffer, 0, 4096+1);
            ssize_t size = count-num_write;
            if (size > 4096) size = 4096;    //for large file
            fread(buffer, size, 1, file);
            ssize_t temp_size = write_all_to_socket(socket_fd, buffer, size);
            if (temp_size == -1) exit(1);
            if (temp_size == 0) {
                print_connection_closed();
                exit(1);
            }
            num_write += temp_size;
        }

        //close file
        fclose(file);
    }

    //3. cleanup
    //free msg
    free(msg);
    //shutdown: Once the client has sent all the data to the server, it should perform a ‘half close’ by closing the write half of the socket
    if (shutdown(socket_fd, SHUT_WR) != 0) perror(NULL);
}

void read_from_server(char* local, verb operation, int socket_fd) {
    //check RESPONSE
    char * response = malloc(6+1);    //need free
    memset(response, 0, 7);
    
    ssize_t read_response_ret = read_all_from_socket(socket_fd, response, 3);
    if (read_response_ret == 0) {
        print_connection_closed();
        exit(1);
    }
    if (read_response_ret == -1) {
        print_invalid_response();
        exit(1);
    }

    //1. OK
    if (strcmp(response, "OK\n") == 0) {
        //PUT & DELETE - finish
        if (operation == PUT || operation == DELETE) {
            print_success();
            free(response);
            if (shutdown(socket_fd, SHUT_RD) != 0) perror(NULL);
            return;
        }
        //GET
        if (operation == GET) {
            //read [size]
            ssize_t count = get_message_size(socket_fd);

            //open local file to write - handle error
            //Truncate file to zero length or create text file for writing. 
            //a) The stream is positioned at the beginning of the file.
            //b) Any created files will have mode S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
            FILE* file = fopen(local, "w");   //need close

            //read [binary data]
            ssize_t num_read = 0;
            char buffer[4096+1];
            while(num_read < count) {
                memset(buffer, 0, 4096+1);
                ssize_t size = count-num_read;
                if (count-num_read > 4096) size = 4096;    //for large file
                ssize_t temp_size  = read_all_from_socket(socket_fd, buffer, size);
                if (temp_size == -1) {
                    print_invalid_response();
                    exit(1);
                }
                if (temp_size == 0) {
                    print_connection_closed();
                    if (num_read < count) print_too_little_data();
                    exit(1);
                }
                num_read += temp_size;  
                //write to file
                fwrite(buffer, temp_size, 1, file);
            }
            //close file
            fclose(file);

            //check for too much data
            size_t temp_size  = read_all_from_socket(socket_fd, buffer, 2);
            if (temp_size > 0) print_received_too_much_data(); 
        }

        //LIST
        if (operation == LIST) {
            //get [size]
            ssize_t count = get_message_size(socket_fd);

            //read [binary data]
            ssize_t num_read = 0;
            char buffer[4096+1];
            while(num_read < count) {
                memset(buffer, 0, 4096+1);
                ssize_t size = count-num_read;
                if (count-num_read > 4096) size = 4096;    //for large file  
                ssize_t temp_size  = read_all_from_socket(socket_fd, buffer, size);
                if (temp_size == -1) {
                    print_invalid_response();
                    exit(1);
                }
                if (temp_size == 0) {
                    print_connection_closed();
                    if (num_read < count) print_too_little_data();
                    exit(1);
                }
            num_read += temp_size;

            //print to STDOUT
            printf("%s", buffer);
        }

        //For LIST, binary data from the server should be printed to STDOUT, each file on a separate line. 
        printf("\n"); //so no need to fflush

        //check for too much data
        size_t temp_size  = read_all_from_socket(socket_fd, buffer, 2);
        if (temp_size > 0) print_received_too_much_data(); 
        }
    } else {
        read_all_from_socket(socket_fd, response+3, 3);
        //2. non-exist STATUS
        if (strcmp(response, "ERROR\n") != 0) {
            print_invalid_response();
            exit(1);
        } 
        //3. ERROR - header will not larger than 1024 
        char buffer[1025];
        memset(buffer, 0, 1025);
        ssize_t error_msg_ret = read_all_from_socket(socket_fd, buffer, 1024); 
        if (error_msg_ret == -1) {
            print_invalid_response();
            exit(1);
        }
        if (error_msg_ret == 0) {
            print_connection_closed();
            exit(1);
        }
        print_error_message(buffer); 
    }

    //free response
    free(response);
    // shutdown for read:
    if (shutdown(socket_fd, SHUT_RD) != 0) perror(NULL);
}

//From Lab: Charming_Chatroom
int connect_to_server(char *host, char *port) {
    struct addrinfo hints;
    struct addrinfo *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    int s = getaddrinfo(host, port, &hints, &result);
    if (s != 0) {
        //print message
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(1);
    }
    int socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_fd == -1) {
        //print message
        perror(NULL);
        exit(1);
    }
    int connect_result = connect(socket_fd, result->ai_addr, result->ai_addrlen);
    if (connect_result == -1) {
        //print message
        perror(NULL);
        exit(1);
    }
    freeaddrinfo(result);
    return socket_fd;
}
/**
 * Given commandline argc and argv, parses argv.
 *
 * argc argc from main()
 * argv argv from main()
 *
 * Returns char* array in form of {host, port, method, remote, local, NULL}
 * where `method` is ALL CAPS
 */
char **parse_args(int argc, char **argv) {
    if (argc < 3) {
        return NULL;
    }

    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (port == NULL) {
        return NULL;
    }

    char **args = calloc(1, 6 * sizeof(char *));
    args[0] = host;
    args[1] = port;
    args[2] = argv[2];
    char *temp = args[2];
    while (*temp) {
        *temp = toupper((unsigned char)*temp);
        temp++;
    }
    if (argc > 3) {
        args[3] = argv[3];
    }
    if (argc > 4) {
        args[4] = argv[4];
    }

    return args;
}

/**
 * Validates args to program.  If `args` are not valid, help information for the
 * program is printed.
 *
 * args     arguments to parse
 *
 * Returns a verb which corresponds to the request method
 */
verb check_args(char **args) {
    if (args == NULL) {
        print_client_usage();
        exit(1);
    }

    char *command = args[2];

    if (strcmp(command, "LIST") == 0) {
        return LIST;
    }

    if (strcmp(command, "GET") == 0) {
        if (args[3] != NULL && args[4] != NULL) {
            return GET;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "DELETE") == 0) {
        if (args[3] != NULL) {
            return DELETE;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "PUT") == 0) {
        if (args[3] == NULL || args[4] == NULL) {
            print_client_help();
            exit(1);
        }
        return PUT;
    }

    // Not a valid Method
    print_client_help();
    exit(1);
}