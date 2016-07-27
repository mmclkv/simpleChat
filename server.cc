#include <iostream>
#include <set>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils.hpp"

std::set<int> conns;
char buf[BUFFLEN];

void processMsg(int fd) {
    int numOfTotalRecv = 0, numOfCurrentRecv;
    while ((numOfCurrentRecv = recv(fd, 
                                    buf + numOfTotalRecv, 
                                    BUFFLEN - numOfTotalRecv, 
                                    0)) > 0) {
        numOfTotalRecv += numOfCurrentRecv;
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        //error on recv
        reportErr("error on recv()");
    }
    //received the messages successfully
    for (auto connfd: conns) {
        if (connfd != fd) {
            if (send(connfd, buf, numOfTotalRecv, 0) != numOfTotalRecv) {
                reportErr("error on send()");
            }
        }
    }
}

int main() {

    struct sockaddr_in serv_addr, clnt_addr;
    struct epoll_event ev, events[MAX_EVENTS];
    int listenSocket, connectSocket, nfds, epollfd; 
    socklen_t clnt_len;
    // create a socket
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1) {
        reportErr("can't create a socket");
    }

    //bind the socket
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&serv_addr.sin_zero, 0, sizeof(char) * 8);
    if (bind(listenSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        reportErr("can't bind the socket to local address");
    }

    //begin listening
    if (listen(listenSocket, SOMAXCONN) == -1) {
        reportErr("failed to start listening");
    }

    //create an epoll
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        reportErr("error on epoll_create1()");
    }

    //register an event
    ev.events = EPOLLIN;
    ev.data.fd = listenSocket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenSocket, &ev) == -1) {
        reportErr("error on epoll_ctl()");
    }
    
    std::cout << "The server is running..." << std::endl;

    //event loop
    for(;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            reportErr("error on epoll_wait()");
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listenSocket) {
                //new connection comes
                connectSocket = accept(listenSocket, 
                                       (struct sockaddr*)&clnt_addr,
                                       &clnt_len);
                if (connectSocket == -1) {
                    reportErr("error on accept()");
                }
                setnonblocking(connectSocket);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = connectSocket;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connectSocket,
                              &ev) == -1) {
                    reportErr("error on epoll_ctl()");
                }
                conns.insert(connectSocket);
            }
            else if (events[n].events & EPOLLRDHUP) {
                //client has disconnected
                conns.erase(events[n].data.fd);
                if (epoll_ctl(epollfd, EPOLL_CTL_DEL, 
                              events[n].data.fd, &ev) == -1) {
                    reportErr("error on epoll_ctl()");
                }
            }
            else {
                //here comes messages
                processMsg(events[n].data.fd);
            }
        }
    }
}
