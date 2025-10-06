#include <iostream>
#include <WS2tcpip.h>
#include <winsock2.h>
#include <mswsock.h>
#include <process.h>
#include "protocol.h" // protocol.h ����

#pragma comment(lib,"ws2_32.lib")

// --- ����(Recv) ���� ������ ---
unsigned int __stdcall RecvThread(void* pArguments)
{
	SOCKET clientSocket = (SOCKET)pArguments;
	char recvBuf[1024];

	while (true)
	{
		// �����κ��� ���� ��Ŷ�� �޴´�. (������ ����� �м����� ����)
		int recvsize = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
		if (recvsize <= 0)
		{
			std::cout << "�������� ������ ������ϴ�." << std::endl;
			// ���⼭ ���α׷��� ���� �����ϰų�, main �����忡 ���� ��ȣ�� ������ �մϴ�.
			// ������ �����ϰ� recv ������ Ż���մϴ�.
			break;
		}

		// ���� ���� ä�� ��Ŷ�̶�� �����ϰ� ���빰�� ����Ѵ�.
		ChatMessagePacket* receivedPacket = (ChatMessagePacket*)recvBuf;
		receivedPacket->message[recvsize - sizeof(PacketHeader)] = '\0'; // �� ���� ó��

		std::cout << "\n[����]: " << receivedPacket->message << std::endl;
		std::cout << "Enter message: "; // ���� �Է��� ���� ������Ʈ �����
	}
	return 0;
}


int main(void)
{
	WSAData wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
	serverAddr.sin_port = htons(8888);

	if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0)
	{
		std::cout << "connect error!" << std::endl;
		return -1;
	}

	std::cout << "server connected!" << std::endl;

	// ���� ������ ����
	HANDLE hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (void*)clientSocket, 0, NULL);

	// --- �۽�(Send) ���� ���� ������ ---
	while (true)
	{
		char message[256] = { 0, };
		std::cout << "Enter message: ";
		std::cin.getline(message, sizeof(message));

		if (strlen(message) > 0)
		{
			ChatMessagePacket chatPacket;
			chatPacket.header.packetType = PacketType::CHAT_MESSAGE;
			strcpy_s(chatPacket.message, message);
			chatPacket.header.packetSize = sizeof(PacketHeader) + strlen(chatPacket.message) + 1;

			send(clientSocket, (const char*)&chatPacket, chatPacket.header.packetSize, 0);
		}

		if (strcmp(message, "q") == 0)
		{
			break;
		}
	}

	// �����尡 ���� ������ ��� ��ٷ��ִ� ���� �����ϴ�.
	WaitForSingleObject(hRecvThread, INFINITE);
	CloseHandle(hRecvThread);

	closesocket(clientSocket);
	WSACleanup();
}