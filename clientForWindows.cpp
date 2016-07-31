#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define SERV_ADDR "192.168.1.104"
#define SERV_PORT 2007
#define BUFFLEN 1024

#define HEADER_ID '0'
#define HEADER_MSG '1'
#define MAXSLEEP 128

char recvbuf[BUFFLEN], sendbuf[BUFFLEN];

SOCKET ConnectSocket;

int connect_retry(const std::string& userName, int sockfd, const struct sockaddr* addr, socklen_t alen) {
	for (int nsec = 1; nsec <= MAXSLEEP; nsec <<= 1) {
		if (connect(sockfd, addr, alen) == 0) {
			std::cout << "Welcome, " << userName << "!" << std::endl;
			return 0;
		}
		if (nsec <= MAXSLEEP / 2) {
			std::cout << "reconnecting..." << std::endl;
			Sleep(nsec);
		}
	}

	return SOCKET_ERROR;
}

void processRecvBuf(int sockfd) {

	//process incoming messages
	for (;;) {
		if (recv(sockfd, recvbuf, BUFFLEN, 0) == -1) {
			printf("error on recv()");

		}
		std::cout << recvbuf;
	}

}

BOOL CtrlHandler(DWORD fdwCtrlType) {
	int iResult;
	switch (fdwCtrlType) {
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:

			// shutdown the send half of the connection since no more data will be sent
			iResult = shutdown(ConnectSocket, SD_SEND);
			if (iResult == SOCKET_ERROR) {
				printf("shutdown failed: %d\n", WSAGetLastError());
				closesocket(ConnectSocket);
				WSACleanup();
				system("pause");
				return 1;
			}

			closesocket(ConnectSocket);
			WSACleanup();

			break;

		default:
			return FALSE;
	}
}

int main() {

	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE)) {
		std::cout << "error on SetConsoleCtrlHandler()" << std::endl;
		return 1;
	}

	WSADATA wsaData;

	int iResult;
	std::string userName;

	std::cout << "Please input your name: ";
	std::cin >> userName;
	std::cin.ignore();

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		system("pause");
		return 1;
	}

	ConnectSocket = INVALID_SOCKET;

	//create a socket
	ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (ConnectSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		WSACleanup();
		system("pause");
		return 1;
	}

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	iResult = inet_pton(AF_INET, SERV_ADDR, &serv_addr.sin_addr.s_addr);
	if (iResult == -1) {
		printf("Error at inet_pton()\n");
		WSACleanup();
		system("pause");
		return 1;
	}
	serv_addr.sin_port = htons(SERV_PORT);

	//connect to the server
	iResult = connect_retry(userName, ConnectSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if (iResult == SOCKET_ERROR) {
		printf("Error at connect()\n");
		WSACleanup();
		system("pause");
		return 1;
	}

	//inform other clients
	sendbuf[0] = HEADER_ID;
	strncpy_s(sendbuf + 1, BUFFLEN - 1, userName.c_str(), userName.length());
	sendbuf[userName.length() + 1] = '\0';
	if (send(ConnectSocket, sendbuf, userName.length() + 2, 0) == -1) {
		printf("error on send()");
		WSACleanup();
		system("pause");
		return 1;
	}
	sendbuf[0] = HEADER_MSG;
	sendbuf[userName.length() + 1] = ':';

	//create a thread to handle incoming messages
	std::thread recvHandler(processRecvBuf, ConnectSocket);
	recvHandler.detach();

	//event loop for sending messages
	for (;;) {
		fgets(sendbuf + userName.length() + 2, BUFFLEN, stdin);
		if (send(ConnectSocket, sendbuf, BUFFLEN, 0) == -1) {
			printf("error on send()");
			WSACleanup();
			system("pause");
			return 1;
		}
	}

	return 0;
}