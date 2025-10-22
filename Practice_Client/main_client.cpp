#include <iostream>
#include <WS2tcpip.h>
#include <winsock2.h>
#include <mswsock.h>
#include <process.h>
#include "protocol.h" // protocol.h 포함

#pragma comment(lib,"ws2_32.lib")
//
//// --- 수신(Recv) 전용 스레드 ---
//unsigned int __stdcall RecvThread(void* pArguments)
//{
//	SOCKET clientSocket = (SOCKET)pArguments;
//	char recvBuf[1024];
//
//	while (true)
//	{
//		// 서버로부터 오는 패킷을 받는다. (지금은 헤더를 분석하지 않음)
//		int recvsize = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
//		if (recvsize <= 0)
//		{
//			std::cout << "서버와의 연결이 끊겼습니다." << std::endl;
//			// 여기서 프로그램을 강제 종료하거나, main 스레드에 종료 신호를 보내야 합니다.
//			// 지금은 간단하게 recv 루프만 탈출합니다.
//			break;
//		}
//
//		// 받은 것이 채팅 패킷이라고 가정하고 내용물을 출력한다.
//		ChatMessagePacket* receivedPacket = (ChatMessagePacket*)recvBuf;
//		receivedPacket->message[recvsize - sizeof(PacketHeader)] = '\0'; // 널 문자 처리
//
//		std::cout << "\n[상대방]: " << receivedPacket->message << std::endl;
//		std::cout << "Enter message: "; // 다음 입력을 위해 프롬프트 재출력
//	}
//	return 0;
//}

// --- 수신(Recv) 전용 스레드 ---
unsigned int __stdcall RecvThread(void* pArguments)
{
    SOCKET clientSocket = (SOCKET)pArguments;
    char recvBuffer[4096];      // 클라이언트의 수신 버퍼
    int receivedBytes = 0;      // 현재까지 받은 총 바이트 수

    while (true)
    {
        // 버퍼의 남은 공간에 데이터를 받는다.
        int recvSize = recv(clientSocket, &recvBuffer[receivedBytes], sizeof(recvBuffer) - receivedBytes, 0);
        if (recvSize <= 0)
        {
            std::cout << "서버와의 연결이 끊겼습니다." << std::endl;
            // main 스레드가 종료될 수 있도록 루프를 빠져나간다.
            // (실제 게임에서는 재접속 로직 등을 여기에 넣는다)
            return 1;
        }

        receivedBytes += recvSize;

        int processedBytes = 0; // 이번에 처리한 총 바이트 수

        // 버퍼에 완전한 패킷이 있는지 반복해서 확인
        while (receivedBytes > 0)
        {
            // 최소한 헤더는 읽을 수 있는지 확인
            if (receivedBytes < sizeof(PacketHeader))
                break;

            PacketHeader* header = (PacketHeader*)&recvBuffer[processedBytes];

            // 완전한 패킷 하나가 도착했는지 확인
            if (receivedBytes < header->packetSize)
                break;

            //// 채팅 패킷 처리
            //if (header->packetType == PacketType::CHAT_MESSAGE)
            //{
            //    ChatMessagePacket* receivedPacket = (ChatMessagePacket*)&recvBuffer[processedBytes];

            //    // C-style 문자열은 마지막에 널 문자('\0')가 있어야 안전하게 출력할 수 있다.
            //    // 패킷 사이즈에서 헤더 사이즈를 뺀 것이 실제 메시지 데이터의 길이이다.
            //    int messageDataLength = header->packetSize - sizeof(PacketHeader);
            //    // 메시지 배열의 마지막은 항상 널 문자여야 하므로, messageDataLength - 1 위치에 널 문자를 넣어준다.
            //    receivedPacket->message[messageDataLength - 1] = '\0';

            //    std::cout << "\n[상대방]: " << receivedPacket->message << std::endl;
            //    std::cout << "Enter message: "; // 다음 입력을 위해 프롬프트 재출력
            //}

            if (header->packetType == PacketType::CHAT_MESSAGE_BROADCAST)
            {
                ChatMessageBroadcastPacket* receivedPacket = (ChatMessageBroadcastPacket*)&recvBuffer[processedBytes];

                // 널 문자 처리
                receivedPacket->nickname[NICKNAME_LENGTH - 1] = '\0';
                receivedPacket->message[CHAT_LENGTH - 1] = '\0';

                std::cout << "\n[" << receivedPacket->nickname << "]: " << receivedPacket->message << std::endl;
                std::cout << "Enter message: ";
            }
            else if (header->packetType == PacketType::SYSTEM_MESSAGE_BROADCAST)
            {
                SystemMessageBroadcastPacket* receivedPacket = (SystemMessageBroadcastPacket*)&recvBuffer[processedBytes];
                receivedPacket->message[CHAT_LENGTH - 1] = '\0'; // 널 문자 처리

                std::cout << "\n" << receivedPacket->message << std::endl;
                std::cout << "Enter message: ";
            }

            processedBytes += header->packetSize;
            receivedBytes -= header->packetSize;
        }

        // 처리한 데이터를 버퍼에서 제거 (남은 데이터를 버퍼 앞으로 당겨오기)
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

	//// 수신 스레드 생성
	//HANDLE hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (void*)clientSocket, 0, NULL);

	//// --- 송신(Send) 전용 메인 스레드 ---
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

    // --- 닉네임 입력 및 로그인 요청 ---
    std::cout << "Enter your nickname: ";
    char nickname[NICKNAME_LENGTH];
    std::cin.getline(nickname, sizeof(nickname));

    LoginRequestPacket loginPacket;
    loginPacket.header.packetType = PacketType::LOGIN_REQUEST;
    strcpy_s(loginPacket.nickname, nickname);
    loginPacket.header.packetSize = sizeof(PacketHeader) + strlen(loginPacket.nickname) + 1;
    send(clientSocket, (const char*)&loginPacket, loginPacket.header.packetSize, 0);

    // --- 서버의 로그인 응답 대기 ---
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
            return -1; // 프로그램 종료
        }
    }
    else
    {
        std::cout << "Invalid response from server." << std::endl;
        return -1;
    }

    // 로그인 성공 시에만 수신 스레드 생성 및 채팅 시작
    HANDLE hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (void*)clientSocket, 0, NULL);
    // --- 송신(Send) 전용 메인 스레드 ---
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

	// 스레드가 끝날 때까지 잠시 기다려주는 것이 좋습니다.
	WaitForSingleObject(hRecvThread, INFINITE);
	CloseHandle(hRecvThread);

	closesocket(clientSocket);
	WSACleanup();
}