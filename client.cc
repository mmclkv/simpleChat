#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "utils.hpp"

char recvbuf[BUFFLEN], sendbuf[BUFFLEN];

int connect_retry(const char* userName, int sockfd, const struct sockaddr* addr, socklen_t alen) {
    for (int nsec = 1; nsec <= MAXSLEEP; nsec <<= 1) {
        if (connect(sockfd, addr, alen) == 0) {
            std::cout << "Welcome, " << userName << "!" << std::endl;
            return 0;
        }
        if (nsec <= MAXSLEEP / 2) {
            std::cout << "reconnecting..." << std::endl;
            sleep(nsec);
        }
    }   

    return -1;
}

void processRecvBuf(int sockfd) {
    
    //process incoming messages
    for (;;) {
       if (recv(sockfd, recvbuf, BUFFLEN, 0) == -1) {
           reportErr("error on recv()");
       }
       std::cout << recvbuf;
    }
}


int main(int argc, char* argv[]) {

    //create a socket
    int connectSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (connectSocket == -1) {
        reportErr("can't create a socket");
    }
    
    //connect to the server
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, LOCALADDR, &serv_addr.sin_addr.s_addr) == -1) {
        reportErr("can't convert ip address");
    }
    if (connect_retry(argv[1], connectSocket, (struct sockaddr*)&serv_addr, 
                      sizeof(serv_addr)) == -1) {
        reportErr("can't connect to the server");
    }
    
    //inform other clients
    sendbuf[0] = HEADER_ID;
    strncpy(sendbuf + 1, argv[1], strlen(argv[1]));
    sendbuf[strlen(argv[1]) + 1] = '\0';
    if (send(connectSocket, sendbuf, strlen(argv[1]) + 2, 0) == -1) {
        reportErr("error on send()");
    }
    sendbuf[0] = HEADER_MSG;
    sendbuf[strlen(argv[1]) + 1] = ':';

    //create a thread to handle incoming messages
    std::thread recvHandler(processRecvBuf, connectSocket);
    recvHandler.detach();

    //event loop for sending messages
    for (;;) {
        fgets(sendbuf + strlen(argv[1]) + 2, BUFFLEN, stdin);
        if (send(connectSocket, sendbuf, BUFFLEN, 0) == -1) {
            reportErr("error on send()");
        }
    }
}
