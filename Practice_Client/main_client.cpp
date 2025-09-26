#include <iostream>
#include <WS2tcpip.h>
#include <winsock2.h>
#include <mswsock.h>

#pragma comment(lib,"ws2_32.lib")

int main(void)
{
	WSAData wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN serverAddr;
	memset(&serverAddr, NULL, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
	serverAddr.sin_port = htons(8888);

	int connectRetval = connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (connectRetval != 0)
	{
		std::cout << "connect error!" << std::endl;
	}


	while (1)
	{
		char message[256]; // 채팅 메시지 버퍼 (최대 255자)
		std::cout << "Enter message: ";
		std::cin.getline(message, sizeof(message)); // 한 줄 입력 받기

		if (strlen(message) > 0)
		{
			send(clientSocket, message, strlen(message), 0);
		}

		char recvBuf[1024];
		int recvsize = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
		if (recvsize > 0)
		{
			recvBuf[recvsize] = '\0';
			std::cout << recvBuf << std::endl;
		}

		else
		{
			std::cout << "server disconnected" << std::endl;
			break;
		}
	}

	closesocket(clientSocket);
	WSACleanup();
}