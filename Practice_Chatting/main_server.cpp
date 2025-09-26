#include <iostream>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <vector>
#include <thread>

#pragma comment(lib,"ws2_32.lib")

// 서버

struct Session
{
	SOCKET socket;
	char buf[1024];
};

std::vector<Session*> SessionList;

// 스레드가 실행할 함수의 원형
// 반환 타입: unsigned int, 호출 규약: __stdcall, 인자: void 포인터 하나
unsigned int __stdcall ThreadMain(void* pArguments)
{
	std::cout << "접속성공";
	// 1. main 스레드로부터 넘겨받은 인자(클라이언트 소켓)를 원래 타입으로 되돌린다.
	Session* clientSession = (Session*)pArguments;

	// 2. 이 스레드는 이제 이 clientSocket하고만 통신하며 자기 할 일을 한다.
	// (예: recv, send 반복...)
	while (true)
	{
		int recvsize = recv(clientSession->socket, clientSession->buf, sizeof(clientSession->buf), NULL);

		// 클라 접속 종료
		if (recvsize <= 0)
		{
			std::cout << "클라 접속 종료" << std::endl;
			break;
		}

		int sendsize = send(clientSession->socket, clientSession->buf, recvsize, NULL);
	}
	// 3. 스레드가 종료되기 전에 자원을 정리한다.
	closesocket(clientSession->socket);
	// lock
	//SessionList에서 해당 Session을 찾아서 지워야함
	//SessionList.
	// unlock
	
	// 4. 스레드를 종료한다.
	return 0;
}

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

	while (1)
	{
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

		Session* newSession = new Session;
		newSession->socket = ClientSocket;

		// Lock
		SessionList.push_back(newSession);
		// unlock 

		// Session당 thread 하나씩 할당
		HANDLE hThread = (HANDLE)_beginthreadex(
			NULL,             // 보안 속성 (보통 NULL)
			0,                // 스택 크기 (0 = 기본값)
			ThreadMain,       // 스레드가 실행할 함수 주소
			(void*)newSession, // 스레드 함수에 전달할 인자
			0,                // 생성 플래그 (0 = 즉시 실행)
			NULL              // 스레드 ID를 받을 변수 주소 (보통 NULL)
		);

		//while을 나가는 조건은 뭐로 해야할까?
	}

	
	closesocket(listenSocket);
	WSACleanup();
}