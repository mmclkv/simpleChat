#ifndef _CHAT_UTILS_
#define _CHAT_UTILS_

#include <fcntl.h>

#define PORT 2007
#define MAX_EVENTS 20
#define BUFFLEN 1024
#define LOCALADDR "127.0.0.1"

void reportErr(const char* message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void setnonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) {
        reportErr("error on F_GETFL");
    }
    flag |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flag) == -1) {
        reportErr("error on F_SETFL");
    }
}

#endif
