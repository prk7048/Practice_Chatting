#include <iostream>
#include <WS2tcpip.h>
#include <winsock2.h>
#include <mswsock.h>
#include <process.h>
#include "protocol.h" // protocol.h ����

#pragma comment(lib,"ws2_32.lib")
//
//// --- ����(Recv) ���� ������ ---
//unsigned int __stdcall RecvThread(void* pArguments)
//{
//	SOCKET clientSocket = (SOCKET)pArguments;
//	char recvBuf[1024];
//
//	while (true)
//	{
//		// �����κ��� ���� ��Ŷ�� �޴´�. (������ ����� �м����� ����)
//		int recvsize = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
//		if (recvsize <= 0)
//		{
//			std::cout << "�������� ������ ������ϴ�." << std::endl;
//			// ���⼭ ���α׷��� ���� �����ϰų�, main �����忡 ���� ��ȣ�� ������ �մϴ�.
//			// ������ �����ϰ� recv ������ Ż���մϴ�.
//			break;
//		}
//
//		// ���� ���� ä�� ��Ŷ�̶�� �����ϰ� ���빰�� ����Ѵ�.
//		ChatMessagePacket* receivedPacket = (ChatMessagePacket*)recvBuf;
//		receivedPacket->message[recvsize - sizeof(PacketHeader)] = '\0'; // �� ���� ó��
//
//		std::cout << "\n[����]: " << receivedPacket->message << std::endl;
//		std::cout << "Enter message: "; // ���� �Է��� ���� ������Ʈ �����
//	}
//	return 0;
//}

// --- ����(Recv) ���� ������ ---
unsigned int __stdcall RecvThread(void* pArguments)
{
    SOCKET clientSocket = (SOCKET)pArguments;
    char recvBuffer[4096];      // Ŭ���̾�Ʈ�� ���� ����
    int receivedBytes = 0;      // ������� ���� �� ����Ʈ ��

    while (true)
    {
        // ������ ���� ������ �����͸� �޴´�.
        int recvSize = recv(clientSocket, &recvBuffer[receivedBytes], sizeof(recvBuffer) - receivedBytes, 0);
        if (recvSize <= 0)
        {
            std::cout << "�������� ������ ������ϴ�." << std::endl;
            // main �����尡 ����� �� �ֵ��� ������ ����������.
            // (���� ���ӿ����� ������ ���� ���� ���⿡ �ִ´�)
            return 1;
        }

        receivedBytes += recvSize;

        int processedBytes = 0; // �̹��� ó���� �� ����Ʈ ��

        // ���ۿ� ������ ��Ŷ�� �ִ��� �ݺ��ؼ� Ȯ��
        while (receivedBytes > 0)
        {
            // �ּ��� ����� ���� �� �ִ��� Ȯ��
            if (receivedBytes < sizeof(PacketHeader))
                break;

            PacketHeader* header = (PacketHeader*)&recvBuffer[processedBytes];

            // ������ ��Ŷ �ϳ��� �����ߴ��� Ȯ��
            if (receivedBytes < header->packetSize)
                break;

            //// ä�� ��Ŷ ó��
            //if (header->packetType == PacketType::CHAT_MESSAGE)
            //{
            //    ChatMessagePacket* receivedPacket = (ChatMessagePacket*)&recvBuffer[processedBytes];

            //    // C-style ���ڿ��� �������� �� ����('\0')�� �־�� �����ϰ� ����� �� �ִ�.
            //    // ��Ŷ ������� ��� ����� �� ���� ���� �޽��� �������� �����̴�.
            //    int messageDataLength = header->packetSize - sizeof(PacketHeader);
            //    // �޽��� �迭�� �������� �׻� �� ���ڿ��� �ϹǷ�, messageDataLength - 1 ��ġ�� �� ���ڸ� �־��ش�.
            //    receivedPacket->message[messageDataLength - 1] = '\0';

            //    std::cout << "\n[����]: " << receivedPacket->message << std::endl;
            //    std::cout << "Enter message: "; // ���� �Է��� ���� ������Ʈ �����
            //}

            if (header->packetType == PacketType::CHAT_MESSAGE_BROADCAST)
            {
                ChatMessageBroadcastPacket* receivedPacket = (ChatMessageBroadcastPacket*)&recvBuffer[processedBytes];

                // �� ���� ó��
                receivedPacket->nickname[NICKNAME_LENGTH - 1] = '\0';
                receivedPacket->message[CHAT_LENGTH - 1] = '\0';

                std::cout << "\n[" << receivedPacket->nickname << "]: " << receivedPacket->message << std::endl;
                std::cout << "Enter message: ";
            }
            else if (header->packetType == PacketType::SYSTEM_MESSAGE_BROADCAST)
            {
                SystemMessageBroadcastPacket* receivedPacket = (SystemMessageBroadcastPacket*)&recvBuffer[processedBytes];
                receivedPacket->message[CHAT_LENGTH - 1] = '\0'; // �� ���� ó��

                std::cout << "\n" << receivedPacket->message << std::endl;
                std::cout << "Enter message: ";
            }

            processedBytes += header->packetSize;
            receivedBytes -= header->packetSize;
        }

        // ó���� �����͸� ���ۿ��� ���� (���� �����͸� ���� ������ ��ܿ���)
        if (processedBytes > 0 && receivedBytes > 0)
        {
            memmove(recvBuffer, &recvBuffer[processedBytes], receivedBytes);
        }
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

	//std::cout << "server connected!" << std::endl;

	//// ���� ������ ����
	//HANDLE hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (void*)clientSocket, 0, NULL);

	//// --- �۽�(Send) ���� ���� ������ ---
	//while (true)
	//{
	//	char message[256] = { 0, };
	//	std::cout << "Enter message: ";
	//	std::cin.getline(message, sizeof(message));

	//	if (strlen(message) > 0)
	//	{
	//		ChatMessagePacket chatPacket;
	//		chatPacket.header.packetType = PacketType::CHAT_MESSAGE;
	//		strcpy_s(chatPacket.message, message);
	//		chatPacket.header.packetSize = sizeof(PacketHeader) + strlen(chatPacket.message) + 1;

	//		send(clientSocket, (const char*)&chatPacket, chatPacket.header.packetSize, 0);
	//	}

	//	if (strcmp(message, "q") == 0)
	//	{
	//		break;
	//	}
	//}

    // ...
    std::cout << "server connected!" << std::endl;

    // --- �г��� �Է� �� �α��� ��û ---
    std::cout << "Enter your nickname: ";
    char nickname[NICKNAME_LENGTH];
    std::cin.getline(nickname, sizeof(nickname));

    LoginRequestPacket loginPacket;
    loginPacket.header.packetType = PacketType::LOGIN_REQUEST;
    strcpy_s(loginPacket.nickname, nickname);
    loginPacket.header.packetSize = sizeof(PacketHeader) + strlen(loginPacket.nickname) + 1;
    send(clientSocket, (const char*)&loginPacket, loginPacket.header.packetSize, 0);

    // --- ������ �α��� ���� ��� ---
    char recvBuf[1024];
    int recvSize = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
    if (recvSize <= 0)
    {
        std::cout << "Server connection lost." << std::endl;
        return -1;
    }

    PacketHeader* header = (PacketHeader*)recvBuf;
    if (header->packetType == PacketType::LOGIN_RESPONSE)
    {
        LoginResponsePacket* responsePacket = (LoginResponsePacket*)recvBuf;
        if (responsePacket->success)
        {
            std::cout << "Login successful. Welcome, " << nickname << "!" << std::endl;
        }
        else
        {
            std::cout << "Login failed. The nickname is already in use." << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return -1; // ���α׷� ����
        }
    }
    else
    {
        std::cout << "Invalid response from server." << std::endl;
        return -1;
    }

    // �α��� ���� �ÿ��� ���� ������ ���� �� ä�� ����
    HANDLE hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (void*)clientSocket, 0, NULL);
    // --- �۽�(Send) ���� ���� ������ ---
    while (true)
    {
        char message[CHAT_LENGTH] = { 0, };
        std::cout << "Enter message: ";
        std::cin.getline(message, sizeof(message));

        if (strlen(message) > 0)
        {
            ChatMessageRequestPacket chatPacket;
            chatPacket.header.packetType = PacketType::CHAT_MESSAGE_REQUEST;
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