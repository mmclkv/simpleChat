#include <iostream>
#include <map>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils.hpp"

std::map<int, std::string> conns;
char buf[BUFFLEN];
const char welcome[] = " has entered the chatting room\n";

struct sockaddr_in serv_addr, clnt_addr;
struct epoll_event ev, events[MAX_EVENTS];
int listenSocket, connectSocket, nfds, epollfd; 
socklen_t clnt_len;
int n;

void processMsg(int fd) {
    int numOfTotalRecv = 0, numOfCurrentRecv;
    while ((numOfCurrentRecv = recv(fd, 
                                    buf + numOfTotalRecv, 
                                    BUFFLEN - numOfTotalRecv, 
                                    0)) > 0) {
        numOfTotalRecv += numOfCurrentRecv;
    }
    if (numOfCurrentRecv == 0) {
        //client has disconnected decently
        std::cout << "client " << conns[fd] << " has disconnected decently" 
                  << std::endl;
        conns.erase(fd);
        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, 
                      fd, &ev) == -1) {
            reportErr("error on epoll_ctl()");
        }
        shutdown(fd, SHUT_WR);
    }
    else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        //client has disconnected indecently
        std::cout << "client " << conns[fd] << " has disconnected indecently" 
                  << std::endl;
        conns.erase(fd);
        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, 
                      fd, &ev) == -1) {
            reportErr("error on epoll_ctl()");
        }
        shutdown(fd, SHUT_WR);
    }
    else {
        //received the messages successfully
        //confirm the type of the message
        if (buf[0] == HEADER_ID) {
            //register the id and inform other clients
            conns[fd] = buf + 1;
            int nameLen = strlen(buf + 1);
            strncpy(buf + 1 + nameLen, welcome, sizeof(welcome));
            for (auto& connPair : conns) {
                if (connPair.first != fd) {
                    if (send(connPair.first, buf + 1, 
                             nameLen + sizeof(welcome), 0) == -1) {
                        reportErr("error on send()");
                    }
                }
            }
            
        }
        else if (buf[0] == HEADER_MSG) {
            for (auto& connPair: conns) {
                if (connPair.first != fd) {
                    if (send(connPair.first, buf + 1, numOfTotalRecv - 1, 0) 
                        != numOfTotalRecv - 1) {
                        reportErr("error on send()");
                    }
                }
            }
        }
    }
}

int main() {

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

        for (n = 0; n < nfds; ++n) {
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
                conns.insert(std::pair<int, std::string>(connectSocket, ""));
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
