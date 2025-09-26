#include <iostream>
#include <WS2tcpip.h>
#include <winsock2.h>
#include <mswsock.h>
#include <process.h>

#pragma comment(lib,"ws2_32.lib")
// 스레드가 실행할 함수
unsigned int __stdcall RecvThread(void* pArguments)
{
	SOCKET clientSocket = (SOCKET)pArguments;
	char recvBuf[1024];

	while (true)
	{
		int recvsize = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);
		if (recvsize <= 0)
		{
			std::cout << "서버와의 연결이 끊겼습니다." << std::endl;
			break;
		}

		recvBuf[recvsize] = '\0';
		std::cout << "\n[상대방]: " << recvBuf << std::endl;
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
		char message[256];
		std::cout << "Enter message: ";
		std::cin.getline(message, sizeof(message));

		if (strlen(message) > 0)
		{
			send(clientSocket, message, strlen(message), 0);
		}

		// "q"를 입력하면 종료
		if (strcmp(message, "q") == 0)
		{
			break;
		}
	}

	closesocket(clientSocket);
	WSACleanup();
}