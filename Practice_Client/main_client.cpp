#include <iostream>
#include <WS2tcpip.h>
#include <winsock2.h>
#include <mswsock.h>
#include <process.h>

#include "protocol.h"
#pragma comment(lib,"ws2_32.lib")
// �����尡 ������ �Լ�
unsigned int __stdcall RecvThread(void* pArguments)
{
	SOCKET clientSocket = (SOCKET)pArguments;
	char recvBuf[1024];

	while (true)
	{
		int recvsize = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
		if (recvsize <= 0)
		{
			std::cout << "�������� ������ ������ϴ�." << std::endl;
			break;
		}

		recvBuf[recvsize] = '\0';
		std::cout << "\n[����]: " << recvBuf << std::endl;
		std::cout << "Enter message: ";
	}

	return 0;
}


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
		return -1;
	}


	std::cout << "server connet!" << std::endl;
	HANDLE hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (void*)clientSocket, 0, NULL);

	while (true)
	{
		char message[256] = { 0, }; // ���۸� 0���� �ʱ�ȭ�ϴ� ����
		std::cout << "Enter message: ";
		std::cin.getline(message, sizeof(message));

		if (strlen(message) > 0)
		{
			// 1. ���� ��Ŷ�� �����ϰ� ������ ä���.
			ChatMessagePacket chatPacket;
			chatPacket.header.packetType = PacketType::CHAT_MESSAGE;
			strcpy_s(chatPacket.message, message); // ������ ���ڿ� ���� �Լ� ���

			// 2. ����� ���� ��Ŷ ũ�⸦ ����Ѵ�.
			//    ��� ũ�� + �޽��� ���ڿ� ���� (+1�� �� ���ڸ� ����)
			chatPacket.header.packetSize = sizeof(PacketHeader) + strlen(chatPacket.message) + 1;

			// 3. ��Ŷ ��ü�� ������ �����Ѵ�.
			//    char*�� ����ȯ�Ͽ� ����Ʈ ��Ʈ������ ����Ѵ�.
			send(clientSocket, (const char*)&chatPacket, chatPacket.header.packetSize, 0);
		}

		// "q"�� �Է��ϸ� ����
		if (strcmp(message, "q") == 0)
		{
			break;
		}
	}

	closesocket(clientSocket);
	WSACleanup();
}