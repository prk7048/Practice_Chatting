#include <iostream>
#include <WS2tcpip.h>
#include <winsock2.h>
#include <mswsock.h>
#include <process.h>
#include "protocol.h" // protocol.h 포함

#pragma comment(lib,"ws2_32.lib")

// --- 수신(Recv) 전용 스레드 ---
unsigned int __stdcall RecvThread(void* pArguments)
{
	SOCKET clientSocket = (SOCKET)pArguments;
	char recvBuf[1024];

	while (true)
	{
		// 서버로부터 오는 패킷을 받는다. (지금은 헤더를 분석하지 않음)
		int recvsize = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
		if (recvsize <= 0)
		{
			std::cout << "서버와의 연결이 끊겼습니다." << std::endl;
			// 여기서 프로그램을 강제 종료하거나, main 스레드에 종료 신호를 보내야 합니다.
			// 지금은 간단하게 recv 루프만 탈출합니다.
			break;
		}

		// 받은 것이 채팅 패킷이라고 가정하고 내용물을 출력한다.
		ChatMessagePacket* receivedPacket = (ChatMessagePacket*)recvBuf;
		receivedPacket->message[recvsize - sizeof(PacketHeader)] = '\0'; // 널 문자 처리

		std::cout << "\n[상대방]: " << receivedPacket->message << std::endl;
		std::cout << "Enter message: "; // 다음 입력을 위해 프롬프트 재출력
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

	// 수신 스레드 생성
	HANDLE hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (void*)clientSocket, 0, NULL);

	// --- 송신(Send) 전용 메인 스레드 ---
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

	// 스레드가 끝날 때까지 잠시 기다려주는 것이 좋습니다.
	WaitForSingleObject(hRecvThread, INFINITE);
	CloseHandle(hRecvThread);

	closesocket(clientSocket);
	WSACleanup();
}