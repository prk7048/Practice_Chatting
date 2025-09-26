#include <iostream>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>

#pragma comment(lib,"ws2_32.lib")

// 辑滚

int main(void)
{
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cout << "WSAStartup err";
		return -1;
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (listenSocket == INVALID_SOCKET)
	{
		std::cout << "listenSocket err";
		return -1;
	}

	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htons(INADDR_ANY);
	serverAddr.sin_port = htons(8888);

	if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cout << "bind err";
		return -1;
	}

	if (listen(listenSocket, 10) == SOCKET_ERROR)
	{
		std::cout << "listen err";
		return -1;
	}

	SOCKADDR_IN clientAddr;
	int addrlen = sizeof(clientAddr);
	memset(&clientAddr, 0, sizeof(clientAddr));
	SOCKET ClientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrlen);
	if (ClientSocket == INVALID_SOCKET)
	{
		std::cout << "Accept Error: " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return -1;
	}

	std::cout << "立加己傍";
	char recvBuf[1024] = { 0, };
	char sendBuf[1024] = { 0, };
	while (1)
	{
		int recvsize = recv(ClientSocket, recvBuf, sizeof(recvBuf), NULL);
		if (recvsize == 0)
		{/*俊矾贸府*/
		}

		int sendsize = send(ClientSocket, recvBuf, recvsize, NULL);
	}
	closesocket(listenSocket);
	closesocket(ClientSocket);
	WSACleanup();
}