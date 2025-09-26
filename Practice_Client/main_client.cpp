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
		char message[256]; // ä�� �޽��� ���� (�ִ� 255��)
		std::cout << "Enter message: ";
		std::cin.getline(message, sizeof(message)); // �� �� �Է� �ޱ�

		char sendBuffer[1024];
		memcpy(sendBuffer, &message, sizeof(message));
		send(clientSocket, sendBuffer, strlen(message), 0);

		char recvBuf[1024];
		int recvsize = recv(clientSocket, recvBuf, sizeof(recvBuf), NULL);

		std::cout << recvBuf << std::endl;
	}

	closesocket(clientSocket);
	WSACleanup();
}